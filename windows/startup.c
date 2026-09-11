#include "startup.h"

#include <gio/gio.h>

#define GTV_TASK_NAME "GoTiengViet"
#define GTV_RUN_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define GTV_RUN_VALUE "GoTiengViet"

static void run_key_set(gboolean enable) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, GTV_RUN_KEY, 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
        return;
    if (enable) {
        WCHAR path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        RegSetValueExW(hkey, L"GoTiengViet", 0, REG_SZ, (const BYTE *)path,
                       (DWORD)(lstrlenW(path) + 1) * sizeof(WCHAR));
    } else {
        RegDeleteValueW(hkey, L"GoTiengViet");
    }
    RegCloseKey(hkey);
}

static gboolean run_key_get(void) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, GTV_RUN_KEY, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return FALSE;
    char path[MAX_PATH];
    DWORD size = sizeof(path);
    LONG res = RegQueryValueEx(hkey, GTV_RUN_VALUE, NULL, NULL, (LPBYTE)path, &size);
    RegCloseKey(hkey);
    return res == ERROR_SUCCESS;
}

/* schtasks without a shell or console window, like engine/ai.c runs curl. */
static gboolean schtasks_run(char **args, gchar **out_stdout) {
    if (out_stdout) *out_stdout = NULL;
    GSubprocess *proc = g_subprocess_newv((const gchar * const *)args,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    if (!proc) return FALSE;
    gchar *output = NULL;
    gboolean ok = g_subprocess_communicate_utf8(proc, NULL, NULL, &output, NULL, NULL);
    if (!ok) g_subprocess_force_exit(proc);
    g_subprocess_wait(proc, NULL, NULL);
    if (!ok || !g_subprocess_get_successful(proc)) {
        g_free(output);
        g_object_unref(proc);
        return FALSE;
    }
    g_object_unref(proc);
    if (out_stdout) *out_stdout = output;
    else g_free(output);
    return TRUE;
}

static gboolean admin_task_exists(void) {
    char *args[] = {"schtasks", "/Query", "/TN", GTV_TASK_NAME, "/XML", NULL};
    gchar *xml = NULL;
    if (!schtasks_run(args, &xml)) return FALSE;
    gboolean highest = xml && strstr(xml, "HighestAvailable") != NULL;
    g_free(xml);
    return highest;
}

GtvStartupMode gtv_startup_get(void) {
    if (admin_task_exists()) return GTV_STARTUP_ADMIN;
    if (run_key_get()) return GTV_STARTUP_USER;
    return GTV_STARTUP_NONE;
}

void gtv_startup_set_user(gboolean enable) {
    run_key_set(enable);
}

gboolean gtv_startup_is_elevated(void) {
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID group = NULL;
    BOOL elevated = FALSE;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &group)) {
        CheckTokenMembership(NULL, group, &elevated);
        FreeSid(group);
    }
    return elevated;
}

static void worker_error(const gchar *utf8) {
    gunichar2 *w = g_utf8_to_utf16(utf8 ? utf8 : "Lỗi không rõ.", -1, NULL, NULL, NULL);
    gunichar2 *t = g_utf8_to_utf16("GoTiengViet", -1, NULL, NULL, NULL);
    if (w && t) MessageBoxW(NULL, (LPCWSTR)w, (LPCWSTR)t, MB_OK | MB_ICONWARNING);
    g_free(w);
    g_free(t);
}

/* Runs elevated (the caller got UAC consent via runas). schtasks inherits
 * our user, so /SC ONLOGON needs no password prompt in the usual
 * same-account UAC case. */
int gtv_startup_apply(const char *op) {
    gboolean to_admin = !strcmp(op, "admin");
    gboolean to_user = !strcmp(op, "user");
    gboolean to_off = !strcmp(op, "admin-off");
    if (!to_admin && !to_user && !to_off) return 2;

    if (to_admin || to_user) {
        /* Never double-start: admin task wins, Run value goes away. */
        if (to_admin) {
            WCHAR wexe[MAX_PATH];
            gchar *exe = NULL, *quoted = NULL;
            gboolean ok = FALSE;
            if (GetModuleFileNameW(NULL, wexe, MAX_PATH)) {
                exe = g_utf16_to_utf8(wexe, -1, NULL, NULL, NULL);
                quoted = g_strdup_printf("\"%s\"", exe ? exe : "");
                char *args[] = {"schtasks", "/Create", "/TN", GTV_TASK_NAME,
                                "/TR", quoted, "/SC", "ONLOGON",
                                "/RL", "HIGHEST", "/F", NULL};
                ok = schtasks_run(args, NULL);
                g_free(exe);
                g_free(quoted);
            }
            if (!ok) {
                worker_error("Không tạo được tác vụ khởi động admin. Hãy thử lại "
                             "(cần đồng ý UAC bằng tài khoản admin của chính bạn).");
                return 1;
            }
            run_key_set(FALSE);
            return 0;
        }
        /* "user": drop the admin task, fall through to Run value below. */
        {
            char *args[] = {"schtasks", "/Delete", "/TN", GTV_TASK_NAME, "/F", NULL};
            if (!schtasks_run(args, NULL)) {
                worker_error("Không gỡ được tác vụ khởi động admin.");
                return 1;
            }
        }
    } else {
        char *args[] = {"schtasks", "/Delete", "/TN", GTV_TASK_NAME, "/F", NULL};
        if (!schtasks_run(args, NULL)) {
            worker_error("Không gỡ được tác vụ khởi động admin.");
            return 1;
        }
        return 0;
    }

    run_key_set(TRUE);
    return 0;
}

void gtv_startup_request(HWND hwnd, const char *op) {
    WCHAR wexe[MAX_PATH];
    if (!GetModuleFileNameW(NULL, wexe, MAX_PATH)) return;
    gchar *exe = g_utf16_to_utf8(wexe, -1, NULL, NULL, NULL);
    gchar *params = g_strdup_printf("--configure-startup %s", op);
    gunichar2 *wparams = g_utf8_to_utf16(params, -1, NULL, NULL, NULL);
    HINSTANCE rc = NULL;
    if (exe && wparams)
        rc = ShellExecuteW(hwnd, L"runas", wexe, (LPCWSTR)wparams, NULL, SW_HIDE);
    if ((UINT_PTR)rc <= 32) {
        worker_error("Cần đồng ý hộp thoại UAC để đổi chế độ khởi động admin. "
                     "Không có gì thay đổi.");
    }
    g_free(exe);
    g_free(params);
    g_free(wparams);
}
