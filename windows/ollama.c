#include "ollama.h"
#include "internal.h"

#include <windows.h>
#include <glib/gstdio.h>

#define OLLAMA_INSTALL_PS1 "irm https://ollama.com/install.ps1 | iex"

gchar *gtv_ollama_log_path(void) {
    WCHAR wtmp[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, wtmp);
    if (n == 0 || n >= MAX_PATH) {
        const gchar *t = g_get_tmp_dir();
        return g_build_filename(t, "ollama_serve.log", NULL);
    }
    gchar *tmp = g_utf16_to_utf8(wtmp, -1, NULL, NULL, NULL);
    if (!tmp) {
        const gchar *t = g_get_tmp_dir();
        return g_build_filename(t, "ollama_serve.log", NULL);
    }
    gchar *path = g_build_filename(tmp, "ollama_serve.log", NULL);
    g_free(tmp);
    return path;
}

void gtv_ollama_log(const gchar *line) {
    if (!line) return;
    gchar *path = gtv_ollama_log_path();
    FILE *f = g_fopen(path, "ab");
    g_free(path);
    if (!f) return;
    fputs(line, f);
    fputc('\n', f);
    fclose(f);
}

/* Full path to ollama.exe: PATH first, then the official per-user install
 * dir (the NSIS installer does not always refresh our PATH). */
static gchar *ollama_exe_path(void) {
    gchar *p = g_find_program_in_path("ollama");
    if (p) return p;
    p = g_find_program_in_path("ollama.exe");
    if (p) return p;
    const gchar *local = g_getenv("LOCALAPPDATA");
    if (local && *local) {
        gchar *def = g_build_filename(local, "Programs", "Ollama", "ollama.exe", NULL);
        if (g_file_test(def, G_FILE_TEST_IS_EXECUTABLE)) return def;
        g_free(def);
    }
    const gchar *prog = g_getenv("ProgramFiles");
    if (prog && *prog) {
        gchar *def = g_build_filename(prog, "Ollama", "ollama.exe", NULL);
        if (g_file_test(def, G_FILE_TEST_IS_EXECUTABLE)) return def;
        g_free(def);
    }
    return NULL;
}

static gint port_from_url(const gchar *url) {
    if (!url || !*url) return 55602;
    GUri *uri = g_uri_parse(url, G_URI_FLAGS_NONE, NULL);
    gint port = -1;
    if (uri) {
        port = g_uri_get_port(uri);
        g_uri_unref(uri);
    }
    if (port < 1024 || port > 65535) {
        if (url && strstr(url, "11434")) return 11434;
        return 55602;
    }
    return port;
}

static gboolean serving_at(const gchar *url) {
    GtvConfig cfg = {0};
    cfg.ai_enabled = TRUE;
    cfg.url = (gchar *)url;
    cfg.model = (gchar *)"probe";
    return gtv_ai_available(&cfg);
}

typedef struct { gchar *url; HWND hwnd; } StatusJob;

static gpointer status_worker(gpointer data) {
    StatusJob *job = data;
    gboolean ok = serving_at(job->url);
    if (job->hwnd && IsWindow(job->hwnd))
        PostMessage(job->hwnd, WM_GTV_OLLAMA_STATUS, (WPARAM)ok, 0);
    g_free(job->url);
    g_free(job);
    return NULL;
}

/* Fire-and-forget availability probe for the setup dialog status line. */
void gtv_ollama_probe_async(HWND hwnd, const gchar *url) {
    StatusJob *job = g_new0(StatusJob, 1);
    job->url = g_strdup(url ? url : "");
    job->hwnd = hwnd;
    GThread *th = g_thread_new("gtv-ollama-probe", status_worker, job);
    if (th) g_thread_unref(th);
    else { g_free(job->url); g_free(job); }
}

/* Build a child environment block = parent + OLLAMA_HOST=127.0.0.1:port. */
static LPWSTR child_env_with_host(gint port) {
    gchar *add = g_strdup_printf("OLLAMA_HOST=127.0.0.1:%d", port);
    gunichar2 *wadd = g_utf8_to_utf16(add, -1, NULL, NULL, NULL);
    g_free(add);
    if (!wadd) return NULL;
    LPWCH parent = GetEnvironmentStringsW();
    size_t parent_len = 0;
    if (parent) {
        LPWCH p = parent;
        while (*p) { size_t l = wcslen(p) + 1; p += l; parent_len += l; }
        parent_len += 1;
    }
    size_t add_len = 0;
    { gunichar2 *p = wadd; while (*p) { add_len++; p++; } add_len += 1; }
    size_t total = parent_len + add_len + 1;
    LPWSTR block = g_new0(WCHAR, total);
    WCHAR *dst = block;
    if (parent) {
        LPWCH p = parent;
        while (*p) { size_t l = wcslen(p) + 1; memcpy(dst, p, l * sizeof(WCHAR)); dst += l; p += l; }
        FreeEnvironmentStringsW(parent);
    } else {
        *dst++ = 0;
    }
    memcpy(dst, wadd, add_len * sizeof(WCHAR));
    dst += add_len;
    *dst = 0;
    g_free(wadd);
    return block;
}

/* Start `ollama serve` detached with output appended to the log. */
static gboolean spawn_serve(const gchar *exe, gint port) {
    gchar *path = gtv_ollama_log_path();
    gunichar2 *wpath = g_utf8_to_utf16(path, -1, NULL, NULL, NULL);
    g_free(path);
    if (!wpath) return FALSE;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hlog = CreateFileW((LPCWSTR)wpath,
        FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    g_free(wpath);
    if (hlog == INVALID_HANDLE_VALUE) return FALSE;

    gunichar2 *wexe = g_utf8_to_utf16(exe, -1, NULL, NULL, NULL);
    if (!wexe) { CloseHandle(hlog); return FALSE; }
    size_t cmdlen = 0;
    { gunichar2 *p = wexe; while (*p++) cmdlen++; }
    WCHAR *cmd = g_new0(WCHAR, cmdlen + 16);
    wsprintfW(cmd, L"\"%ls\" serve", (LPCWSTR)wexe);
    g_free(wexe);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hlog;
    si.hStdError = hlog;
    si.hStdInput = NULL;
    ZeroMemory(&pi, sizeof(pi));
    LPWSTR env = child_env_with_host(port);
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE,
        DETACHED_PROCESS | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        env, NULL, &si, &pi);
    g_free(env);
    g_free(cmd);
    CloseHandle(hlog);
    if (!ok) return FALSE;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return TRUE;
}

/* Phase 2: server answers at url, starting it when needed. */
static gboolean ensure_served(const gchar *url) {
    if (serving_at(url)) {
        gchar *msg = g_strdup_printf("Ollama đang chạy ở %s — sẵn sàng gợi ý.", url);
        gtv_ollama_log(msg);
        g_free(msg);
        return TRUE;
    }
    gchar *exe = ollama_exe_path();
    if (!exe) return FALSE; /* caller installs first */
    gint port = port_from_url(url);
    gchar *hdr = g_strdup_printf("=== ollama serve port %d (GoTiengViet tự khởi động) ===", port);
    gtv_ollama_log(hdr);
    g_free(hdr);
    if (!spawn_serve(exe, port)) {
        gchar *msg = g_strdup_printf("LỖI khởi động 'ollama serve' (mã %lu).", GetLastError());
        gtv_ollama_log(msg);
        g_free(msg);
        g_free(exe);
        return FALSE;
    }
    g_free(exe);
    gtv_ollama_log("Đã chạy ollama serve, chờ /api/tags (tối đa 15s)...");
    for (int i = 0; i < 15; i++) {
        Sleep(1000);
        if (serving_at(url)) {
            gchar *msg = g_strdup_printf("Ollama đã chạy ở port %d — sẵn sàng gợi ý.", port);
            gtv_ollama_log(msg);
            g_free(msg);
            return TRUE;
        }
    }
    gtv_ollama_log("CẢNH BÁO: quá 15s chưa thấy /api/tags. Kiểm tra log, port có bị chiếm không.");
    return FALSE;
}

gint64 gtv_ollama_download_bytes(void) {
    gchar *exe = ollama_exe_path();
    if (exe) { g_free(exe); return -1; } /* already installed */
    return 0;
}

static gchar *win32_run_silent_capture(const WCHAR *cmdline, DWORD timeout_ms) {
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return NULL;
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;
    ZeroMemory(&pi, sizeof(pi));

    WCHAR *cmd = _wcsdup(cmdline);
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        return NULL;
    }

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, timeout_ms);
    CloseHandle(pi.hProcess);

    GString *out = g_string_new("");
    char buf[1024];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        g_string_append(out, buf);
    }
    CloseHandle(hReadPipe);
    return g_string_free(out, FALSE);
}

/* Official installation: irm https://ollama.com/install.ps1 | iex */
static gboolean download_and_install(void) {
    gtv_ollama_log("Không tìm thấy 'ollama' — đang cài đặt tự động (irm https://ollama.com/install.ps1 | iex)...");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    WCHAR cmd[] = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"if ($env:USERPROFILE) { $env:TEMP = Join-Path $env:USERPROFILE 'Downloads'; $env:TMP = $env:TEMP }; irm https://ollama.com/install.ps1 | iex\"";
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        gchar *msg = g_strdup_printf("LỖI chạy PowerShell cài Ollama (mã %lu).", GetLastError());
        gtv_ollama_log(msg);
        g_free(msg);
        return FALSE;
    }

    DWORD wr = WaitForSingleObject(pi.hProcess, 600000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (wr == WAIT_TIMEOUT) {
        gtv_ollama_log("Cài Ollama quá 10 phút chưa xong.");
        return FALSE;
    }
    if (code != 0) {
        gchar *msg = g_strdup_printf("Cài Ollama thất bại (mã %lu).", code);
        gtv_ollama_log(msg);
        g_free(msg);
        return FALSE;
    }
    gtv_ollama_log("Cài Ollama hoàn tất.");
    gchar *check = ollama_exe_path();
    gboolean found = check != NULL;
    g_free(check);
    return found;
}

static gboolean model_name_ok(const gchar *model) {
    if (!model || !*model || !g_strcmp0(model, "rule")) return FALSE;
    for (const gchar *p = model; *p; p++) {
        if (!g_ascii_isalnum(*p) && *p != ':' && *p != '.' && *p != '_' && *p != '-')
            return FALSE;
    }
    return TRUE;
}

/* Phase 3: `ollama pull <model>` when /api/tags lacks it. */
static void ensure_model(const gchar *url, const gchar *model) {
    if (!model_name_ok(model)) return;
    GtvConfig probe = {0};
    probe.ai_enabled = TRUE;
    probe.url = (gchar *)url;
    probe.model = (gchar *)model;
    /* Check /api/tags silently without console window popup. */
    WCHAR tags_cmd[1024];
    _snwprintf(tags_cmd, G_N_ELEMENTS(tags_cmd), L"curl.exe --silent --show-error --fail --max-time 5 \"%hs/api/tags\"", url);
    gchar *out = win32_run_silent_capture(tags_cmd, 6000);
    gboolean present = out && strstr(out, model) != NULL;
    g_free(out);
    (void)probe;
    if (present) {
        gchar *msg = g_strdup_printf("Model %s đã sẵn sàng.", model);
        gtv_ollama_log(msg);
        g_free(msg);
        return;
    }
    gchar *msg = g_strdup_printf("Model %s chưa có — tự tải nền, xong sẽ báo.", model);
    gtv_ollama_log(msg);
    g_free(msg);
    gchar *exe = ollama_exe_path();
    if (!exe) return;
    gint port = port_from_url(url);
    gunichar2 *wexe = g_utf8_to_utf16(exe, -1, NULL, NULL, NULL);
    g_free(exe);
    if (!wexe) return;
    gchar *pull_msg = g_strdup_printf("Đang pull model %s (lần đầu có thể mất vài phút)...", model);
    gtv_ollama_log(pull_msg);
    g_free(pull_msg);
    /* `ollama pull` inherits OLLAMA_HOST so it hits our server port. */
    size_t cmdlen = 0;
    { gunichar2 *p = wexe; while (*p++) cmdlen++; }
    WCHAR *cmd = g_new0(WCHAR, cmdlen + 64 + strlen(model));
    wsprintfW(cmd, L"\"%ls\" pull %hs", (LPCWSTR)wexe, model);
    g_free(wexe);
    /* Redirect pull progress into the same log. */
    gchar *path = gtv_ollama_log_path();
    gunichar2 *wpath = g_utf8_to_utf16(path, -1, NULL, NULL, NULL);
    g_free(path);
    HANDLE hlog = INVALID_HANDLE_VALUE;
    if (wpath) {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        hlog = CreateFileW((LPCWSTR)wpath, FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        g_free(wpath);
    }
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (hlog != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hlog;
        si.hStdError = hlog;
    }
    ZeroMemory(&pi, sizeof(pi));
    LPWSTR env = child_env_with_host(port);
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, hlog != INVALID_HANDLE_VALUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, env, NULL, &si, &pi);
    g_free(env);
    g_free(cmd);
    if (hlog != INVALID_HANDLE_VALUE) CloseHandle(hlog);
    if (!ok) {
        gtv_ollama_log("LỖI chạy 'ollama pull'. Thử tay: ollama pull <model>.");
        return;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    msg = g_strdup_printf(code == 0 ? "Đã tải xong model %s." : "Pull model %s thất bại (mã %lu).", model, code);
    gtv_ollama_log(msg);
    g_free(msg);
}

typedef struct { gchar *url; gchar *model; HWND hwnd; } EnsureJob;
static volatile LONG ensure_in_flight = 0;

static gpointer ensure_worker(gpointer data) {
    EnsureJob *job = data;
    /* Phase 1: installed? */
    gchar *exe = ollama_exe_path();
    if (exe) {
        g_free(exe);
    } else {
        if (!download_and_install()) {
            if (job->hwnd && IsWindow(job->hwnd))
                PostMessage(job->hwnd, WM_GTV_OLLAMA_STATUS, 0, 0);
            goto done;
        }
    }
    /* Phase 2: serving? */
    if (!ensure_served(job->url)) {
        if (job->hwnd && IsWindow(job->hwnd))
            PostMessage(job->hwnd, WM_GTV_OLLAMA_STATUS, 0, 0);
        goto done;
    }
    /* Phase 3: model present? */
    ensure_model(job->url, job->model);
    if (job->hwnd && IsWindow(job->hwnd))
        PostMessage(job->hwnd, WM_GTV_OLLAMA_STATUS, (WPARAM)serving_at(job->url), 0);
done:
    g_free(job->url);
    g_free(job->model);
    g_free(job);
    InterlockedExchange(&ensure_in_flight, 0);
    return NULL;
}

/* Full chain: check → install → serve → pull model. Guarded single-flight. */
void gtv_ollama_ensure_all_async(HWND hwnd, const gchar *url, const gchar *model) {
    if (InterlockedCompareExchange(&ensure_in_flight, 1, 0) != 0) {
        gtv_ollama_log("Đang xử lý Ollama, vui lòng đợi...");
        return;
    }
    EnsureJob *job = g_new0(EnsureJob, 1);
    job->url = g_strdup(url && *url ? url : "http://localhost:55602");
    job->model = g_strdup(model && *model ? model : "qwen2:0.5b");
    job->hwnd = hwnd;
    GThread *th = g_thread_new("gtv-ollama-ensure", ensure_worker, job);
    if (th) g_thread_unref(th);
    else {
        g_free(job->url); g_free(job->model); g_free(job);
        InterlockedExchange(&ensure_in_flight, 0);
    }
}

/* Serve-only path (kept for compatibility). */
void gtv_ollama_ensure_serve_async(const gchar *url) {
    gtv_ollama_ensure_all_async(NULL, url, "rule");
}
