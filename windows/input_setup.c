/* Automatic per-user input setup: attach the GoTV keyboard to the user's
 * Vietnamese language so it shows up in the Win+Space / taskbar switcher
 * without manual Settings work.
 *
 * GoTV is the ONLY keyboard under Vietnamese (badge "VIE"): any other TIP
 * Windows puts there (e.g. the built-in Telex) is removed, otherwise the
 * switcher shows duplicate VIE entries. Other languages (e.g. en-US with
 * the plain US keyboard, badge "ENG") are never touched. Idempotent: a
 * version stamp avoids respawning powershell on every launch. The heavy
 * lifting goes through Set-WinUserLanguageList — the OS-validated path —
 * in a hidden powershell child, because raw registry writes are
 * ignored/dropped by the input stack. */
#include "input_setup.h"
#include "version.h"

#include <windows.h>
#include <glib.h>
#include <glib/gstdio.h>

/* TIP strings, same format Windows itself uses, e.g.
 * 042a:{CLSID}{ProfileGUID}. GoTV lives under Vietnamese so the taskbar
 * badge shows "VIE" — distinguishing it from the plain US keyboard ("ENG"). */
#define GTV_TIP_VI "042a:{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}"

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

/* One powershell invocation: Vietnamese gets GoTV as its ONLY keyboard
 * (badge "VIE"), en-US and friends are untouched (badge "ENG").
 * Result: exactly 2 switcher entries — ENG US keyboard + VIE GoTV. */
static const char *setup_script(void) {
    return "$ErrorActionPreference='SilentlyContinue';"
           "$l=Get-WinUserLanguageList;"
           "$c=$false;"
           /* Add Vietnamese with GoTV-only if missing */
           "$hasVi=$null -ne ($l | Where-Object{$_.LanguageTag -match '^vi'});"
           "if(-not $hasVi){"
           "  $vi=New-WinUserLanguageList 'vi-VN';"
           "  $vi[0].InputMethodTips.Clear();"
           "  $vi[0].InputMethodTips.Add('" GTV_TIP_VI "');"
           "  $l+=$vi;$c=$true}"
           /* Existing vi: strip non-GoTV TIPs (built-in Telex), ensure GoTV */
           "foreach($x in $l){"
           "  if($x.LanguageTag -match '^vi'){"
           "    $drop=@($x.InputMethodTips | Where-Object{$_ -ne '" GTV_TIP_VI "'});"
           "    foreach($d in $drop){[void]$x.InputMethodTips.Remove($d);$c=$true}"
           "    if($x.InputMethodTips -notcontains '" GTV_TIP_VI "'){"
           "      $x.InputMethodTips.Add('" GTV_TIP_VI "');$c=$true}}}"
           "if($c){Set-WinUserLanguageList $l -Force};"
           /* Verify: vi exists with GoTV as its sole keyboard */
           "$l2=Get-WinUserLanguageList;"
           "$ok=$true;"
           "$hasVi2=$null -ne ($l2 | Where-Object{$_.LanguageTag -match '^vi'});"
           "if(-not $hasVi2){$ok=$false}"
           "foreach($x in $l2){"
           "  if($x.LanguageTag -match '^vi'){"
           "    if($x.InputMethodTips -notcontains '" GTV_TIP_VI "'){$ok=$false}"
           "    if($x.InputMethodTips.Count -ne 1){$ok=$false}}}"
           "if($ok){exit 0}else{exit 1}";
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
