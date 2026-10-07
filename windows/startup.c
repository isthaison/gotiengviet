#include "startup.h"
#include "internal.h"

#include <gio/gio.h>

#define GTV_TASK_NAME "GoTiengViet"
#define GTV_RUN_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define GTV_RUN_VALUE "GoTiengViet"

/* Startup entries point at the running (current) payload exe; a stale
 * entry heals itself because old payloads forward to current.txt. */
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

/* Heal a stale Run value (older payload) to this exe. Only touches the
 * value when it already exists and points inside our own app dir. */
void gtv_startup_repoint(void) {
    WCHAR wexe[MAX_PATH];
    if (!GetModuleFileNameW(NULL, wexe, MAX_PATH)) return;
    gchar *exe = g_utf16_to_utf8((const gunichar2 *)wexe, -1, NULL, NULL, NULL);
    if (!exe) return;
    gchar *appdir = gtv_app_dir_for_module(exe);
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, GTV_RUN_KEY, 0, KEY_READ | KEY_SET_VALUE, &hkey) == ERROR_SUCCESS) {
        WCHAR cur[MAX_PATH];
        DWORD size = sizeof(cur), type = 0;
        if (RegQueryValueExW(hkey, L"GoTiengViet", NULL, &type, (LPBYTE)cur, &size) == ERROR_SUCCESS
            && type == REG_SZ) {
            gchar *cur8 = g_utf16_to_utf8((const gunichar2 *)cur, -1, NULL, NULL, NULL);
            if (cur8 && strcmp(cur8, exe) != 0) {
                gchar *curdir = gtv_app_dir_for_module(cur8);
                gboolean same_app = !strcmp(curdir, appdir);
                g_free(curdir);
                if (same_app) {
                    gunichar2 *wpath = g_utf8_to_utf16(exe, -1, NULL, NULL, NULL);
                    if (wpath) {
                        RegSetValueExW(hkey, L"GoTiengViet", 0, REG_SZ, (const BYTE *)wpath,
                                       (DWORD)((lstrlenW((LPCWSTR)wpath) + 1) * sizeof(WCHAR)));
                        g_free(wpath);
                    }
                }
            }
            g_free(cur8);
        }
        RegCloseKey(hkey);
    }
    g_free(appdir);
    g_free(exe);
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

/* schtasks with no window at all: GSubprocess leaves console flashing
 * (each menu open used to blink twice), so spawn raw with CREATE_NO_WINDOW
 * + SW_HIDE and capture stdout through a pipe (same pattern as the
 * update downloader in engine/update.c).
 * Returns TRUE only on exit code 0; stdout (raw bytes, may be UTF-16 XML)
 * goes to *out_stdout when requested. Bounded 20s wait, never fatal. */
static gboolean schtasks_run(char **args, gchar **out_stdout) {
    if (out_stdout) *out_stdout = NULL;
    if (!args || !args[0]) return FALSE;

    /* Resolve the tool inside System32 (no PATH hijack, no shell). */
    gchar *exe = args[0];
    gchar *full = NULL;
    if (!strchr(exe, '/') && !strchr(exe, '\\') && !strchr(exe, ':')) {
        WCHAR sysdir[MAX_PATH];
        if (GetSystemDirectoryW(sysdir, MAX_PATH)) {
            gchar *sys8 = g_utf16_to_utf8((const gunichar2 *)sysdir, -1, NULL, NULL, NULL);
            if (sys8) {
                full = g_strdup_printf("%s\\%s", sys8, exe);
                exe = full;
            }
        }
    }
    GString *line = g_string_new(exe);
    for (int i = 1; args[i]; i++) {
        g_string_append_c(line, ' ');
        g_string_append(line, args[i]);
    }
    g_free(full);
    gunichar2 *wline = g_utf8_to_utf16(line->str, -1, NULL, NULL, NULL);
    g_string_free(line, TRUE);
    if (!wline) return FALSE;

    HANDLE hRead = NULL, hWrite = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        g_free(wline);
        return FALSE;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;
    ZeroMemory(&pi, sizeof(pi));

    BOOL started = CreateProcessW(NULL, (LPWSTR)wline, NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                  NULL, NULL, &si, &pi);
    g_free(wline);
    CloseHandle(hWrite);
    if (!started) {
        CloseHandle(hRead);
        return FALSE;
    }
    CloseHandle(pi.hThread);

    DWORD wait_rc = WaitForSingleObject(pi.hProcess, 20000);
    DWORD exit_code = 1;
    if (wait_rc == WAIT_TIMEOUT)
        TerminateProcess(pi.hProcess, 1);
    else
        GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    GString *out = g_string_new("");
    char buf[1024];
    DWORD got = 0;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &got, NULL) && got > 0) {
        buf[got] = '\0';
        g_string_append(out, buf);
    }
    CloseHandle(hRead);

    if (exit_code != 0) {
        g_string_free(out, TRUE);
        return FALSE;
    }
    if (out_stdout)
        *out_stdout = g_string_free(out, FALSE);
    else
        g_string_free(out, TRUE);
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
                exe = g_utf16_to_utf8((const gunichar2 *)wexe, -1, NULL, NULL, NULL);
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
