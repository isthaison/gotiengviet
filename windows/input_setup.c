/* Automatic per-user input setup: migrate GoTV from Vietnamese to English/US
 * and remove the duplicate plain US keyboard. */
#include "input_setup.h"
#include "version.h"

#include <windows.h>
#include <glib.h>
#include <glib/gstdio.h>

#define GTV_TIP_EN "0409:{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}"
#define GTV_TIP_VI "042a:{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}"
#define GTV_TIP_US "0409:00000409"
#define GTV_INPUT_SETUP_REVISION "6"

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
        ok = !strcmp(g_strstrip(contents), GTV_INPUT_SETUP_REVISION);
    g_free(contents);
    g_free(path);
    return ok;
}

static void stamp_write(void) {
    gchar *path = stamp_path();
    g_file_set_contents(path, GTV_INPUT_SETUP_REVISION, -1, NULL);
    g_free(path);
}

static const char *setup_script(void) {
    return "$ErrorActionPreference='SilentlyContinue';"
           "$l=Get-WinUserLanguageList;"
           "$c=$false;"
           "for($i=$l.Count-1;$i -ge 0;$i--){$x=$l[$i];if($x.LanguageTag -match '^vi' -and $x.InputMethodTips -contains '" GTV_TIP_VI "'){[void]$l.RemoveAt($i);$c=$true}}"
           "$ei=-1;"
           "for($i=0;$i -lt $l.Count;$i++){if($l[$i].LanguageTag -eq 'en-US'){$ei=$i;break}}"
           "if($ei -lt 0){$add=New-WinUserLanguageList 'en-US';$l.Add($add[0]);$ei=$l.Count-1;$c=$true}"
           "$en=$l[$ei];"
           "for($i=$en.InputMethodTips.Count-1;$i -ge 0;$i--){$tip=$en.InputMethodTips[$i];if($tip -eq '" GTV_TIP_US "' -or $tip -eq '" GTV_TIP_EN "'){$en.InputMethodTips.RemoveAt($i);$c=$true}}"
           "if($en.InputMethodTips -notcontains '" GTV_TIP_EN "'){$en.InputMethodTips.Add('" GTV_TIP_EN "');$c=$true}"
           "if($c){Set-WinUserLanguageList $l -Force};"
           "Remove-Item -LiteralPath 'HKCU:\\Software\\Microsoft\\CTF\\Assemblies\\0x0000042a' -Recurse -Force -ErrorAction SilentlyContinue;"
           "$l2=Get-WinUserLanguageList;"
           "$en2=$null;"
           "for($i=0;$i -lt $l2.Count;$i++){if($l2[$i].LanguageTag -eq 'en-US'){$en2=$l2[$i];break}}"
           "$goCount=0;$hasUs=$false;"
           "if($null -ne $en2){for($i=0;$i -lt $en2.InputMethodTips.Count;$i++){if($en2.InputMethodTips[$i] -eq '" GTV_TIP_EN "'){$goCount++}elseif($en2.InputMethodTips[$i] -eq '" GTV_TIP_US "'){$hasUs=$true}}}"
           "$viGone=$true;"
           "for($i=0;$i -lt $l2.Count;$i++){if($l2[$i].LanguageTag -match '^vi' -and $l2[$i].InputMethodTips -contains '" GTV_TIP_VI "'){$viGone=$false;break}}"
           "$ok=$null -ne $en2 -and $goCount -eq 1 -and -not $hasUs -and $viGone;"
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
