/* Automatic per-user input setup: attach the GoTV keyboard to the user's
 * existing languages so it shows up in the Win+Space / taskbar switcher
 * without manual Settings work.
 *
 * Everything here is additive-only (never removes the user's keyboards)
 * and idempotent. The heavy lifting goes through Set-WinUserLanguageList
 * — the OS-validated path — in a hidden powershell child, because raw
 * registry writes are ignored/dropped by the input stack. A version
 * stamp avoids respawning powershell on every launch. */
#include "input_setup.h"
#include "version.h"

#include <windows.h>
#include <glib.h>
#include <glib/gstdio.h>

/* TIP strings, same format Windows itself uses, e.g.
 * 042A:{CLSID}{ProfileGUID}. -notcontains is case-insensitive, so the
 * cmdlet's 042a->042A normalization is harmless. */
#define GTV_TIP_VI "042a:{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}"
#define GTV_TIP_EN "0409:{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}"

static void setup_log(const gchar *fmt, ...) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    g_mkdir_with_parents(dir, 0755);
    gchar *path = g_build_filename(dir, "input-setup.log", NULL);
    g_free(dir);
    FILE *f = g_fopen(path, "a");
    g_free(path);
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

static gchar *stamp_path(void) {
    return g_build_filename(g_get_user_config_dir(), "gotiengviet", "input-setup.done", NULL);
}

static gboolean stamp_current(void) {
    gchar *path = stamp_path();
    gchar *contents = NULL;
    gboolean ok = FALSE;
    if (g_file_get_contents(path, &contents, NULL, NULL) && contents)
        ok = !strcmp(g_strstrip(contents), GTV_VERSION);
    g_free(contents);
    g_free(path);
    return ok;
}

static void stamp_write(void) {
    gchar *path = stamp_path();
    g_file_set_contents(path, GTV_VERSION, -1, NULL);
    g_free(path);
}

/* One powershell invocation: add missing GoTV TIPs, apply if changed,
 * re-read and exit 0 only when every applicable TIP is present.
 * Rule (avoid duplicate switcher entries): if Vietnamese exists, GoTV
 * lives there only; otherwise attach it to en-US so it still shows up. */
static const char *setup_script(void) {
    return "$ErrorActionPreference='SilentlyContinue';"
           "$l=Get-WinUserLanguageList;"
           "$hasVi=$null -ne ($l | Where-Object{$_.LanguageTag -eq 'vi'});"
           "$c=$false;"
           "foreach($x in $l){"
           "  $t=$null;"
           "  if($x.LanguageTag -eq 'vi'){$t='" GTV_TIP_VI "'}"
           "  elseif(-not $hasVi -and $x.LanguageTag -eq 'en-US'){$t='" GTV_TIP_EN "'}"
           "  if($t -and ($x.InputMethodTips -notcontains $t)){"
           "    $x.InputMethodTips.Add($t);$c=$true}}"
           "if($c){Set-WinUserLanguageList $l -Force};"
           "$l2=Get-WinUserLanguageList;"
           "$hasVi2=$null -ne ($l2 | Where-Object{$_.LanguageTag -eq 'vi'});"
           "$ok=$true;"
           "foreach($x in $l2){"
           "  $t=$null;"
           "  if($x.LanguageTag -eq 'vi'){$t='" GTV_TIP_VI "'}"
           "  elseif(-not $hasVi2 -and $x.LanguageTag -eq 'en-US'){$t='" GTV_TIP_EN "'}"
           "  if($t -and ($x.InputMethodTips -notcontains $t)){$ok=$false}}"
           "exit(($ok)?0:1)";
}

static gpointer setup_worker(gpointer data) {
    (void)data;
    setup_log("input setup start version=%s", GTV_VERSION);

    WCHAR sysdir[MAX_PATH];
    WCHAR ps_path[MAX_PATH];
    if (GetSystemDirectoryW(sysdir, MAX_PATH))
        swprintf(ps_path, MAX_PATH, L"%ls\\WindowsPowerShell\\v1.0\\powershell.exe", sysdir);
    else
        wcscpy(ps_path, L"powershell.exe");
    if (GetFileAttributesW(ps_path) == INVALID_FILE_ATTRIBUTES)
        wcscpy(ps_path, L"powershell.exe");

    gchar *script = g_strdup(setup_script());
    gunichar2 *wscript = g_utf8_to_utf16(script, -1, NULL, NULL, NULL);
    g_free(script);
    if (!wscript) {
        setup_log("script conversion failed");
        return NULL;
    }

    size_t cmdlen = wcslen(ps_path) + wcslen((WCHAR *)wscript) + 256;
    WCHAR *cmd = g_new0(WCHAR, cmdlen);
    /* Bypass policy for this one signed-off command line; no profile,
     * non-interactive, hidden window. */
    swprintf(cmd, cmdlen,
             L"\"%ls\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"%ls\"",
             ps_path, (WCHAR *)wscript);
    g_free(wscript);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    BOOL started = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                                  NULL, NULL, &si, &pi);
    g_free(cmd);
    if (!started) {
        setup_log("CreateProcess powershell failed: %lu", (unsigned long)GetLastError());
        return NULL;
    }
    CloseHandle(pi.hThread);
    DWORD wait_rc = WaitForSingleObject(pi.hProcess, 90000);
    DWORD exit_code = 1;
    if (wait_rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        setup_log("powershell timed out after 90s");
    } else {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }
    CloseHandle(pi.hProcess);
    setup_log("powershell done exit=%lu", (unsigned long)exit_code);
    if (exit_code == 0) {
        stamp_write();
        setup_log("input setup verified, stamp written");
    } else {
        setup_log("input setup incomplete (machine registration may be missing); will retry");
    }
    return NULL;
}

void gtv_input_setup_ensure_async(void) {
    if (stamp_current())
        return;
    GThread *th = g_thread_new("gtv-input-setup", setup_worker, NULL);
    if (!th)
        return;
    g_thread_unref(th);
}
