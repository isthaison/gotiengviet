#include "tsf_install.h"

#include <windows.h>
#include <stdio.h>
#include <glib.h>
#include <wchar.h>

static const wchar_t *GTV_TSF_INPROC_KEY =
    L"Software\\Classes\\CLSID\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\InprocServer32";

/* TSF profile keys: HKCU\Software\Microsoft\CTF\TIP\{CLSID}\LanguageProfile\{langid}\{profileguid}
 * If these don't exist, the keyboard won't appear in language settings. */
static const wchar_t *GTV_TSF_PROFILE_BASE =
    L"Software\\Microsoft\\CTF\\TIP\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\LanguageProfile";
static const wchar_t *GTV_TSF_PROFILE_EN = L"0x00000409\\{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}";
static const wchar_t *GTV_TSF_PROFILE_VI = L"0x0000042a\\{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}";

/* TSF category key: HKCU\Software\Microsoft\CTF\TIP\{CLSID}\Category\{categoryGUID}
 * The ITfCategoryMgr API may write to HKLM which fails for per-user installs.
 * We register the keyboard category manually under HKCU to ensure it works. */
static const wchar_t *GTV_TSF_CATEGORY_BASE =
    L"Software\\Microsoft\\CTF\\TIP\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\Category";
/* GUID_TFCAT_TIP_KEYBOARD = {34745C63-B2F0-11D0-98EF-00AA006D2E36} */
static const wchar_t *GTV_TSF_CAT_KEYBOARD =
    L"{34745C63-B2F0-11D0-98EF-00AA006D2E36}\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}";

static gchar *s_log_path = NULL;

static const gchar *get_log_path(void) {
    if (!s_log_path)
        s_log_path = g_build_filename(g_get_user_data_dir(), "gotiengviet",
                                      "tsf_install.log", NULL);
    return s_log_path;
}

static void log_msg(const gchar *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    gchar *msg = g_strdup_vprintf(fmt, args);
    va_end(args);

    gchar *dir = g_path_get_dirname(get_log_path());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);

    FILE *f = fopen(get_log_path(), "a");
    if (f) {
        gchar *ts = g_strdup_printf("[%lu] ", (unsigned long)GetTickCount());
        fputs(ts, f);
        fputs(msg, f);
        fputc('\n', f);
        fclose(f);
        g_free(ts);
    }
    g_free(msg);
}

static gboolean same_path_ci(const wchar_t *a, const wchar_t *b) {
    wchar_t full_a[MAX_PATH];
    wchar_t full_b[MAX_PATH];

    if (!a || !b) return FALSE;
    if (!GetFullPathNameW(a, MAX_PATH, full_a, NULL)) return FALSE;
    if (!GetFullPathNameW(b, MAX_PATH, full_b, NULL)) return FALSE;
    return _wcsicmp(full_a, full_b) == 0;
}

static gboolean registered_to(const wchar_t *dll_path) {
    HKEY hkey = NULL;
    wchar_t registered[MAX_PATH];
    DWORD type = 0;
    DWORD size = sizeof(registered);
    gboolean ok = FALSE;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, GTV_TSF_INPROC_KEY, 0, KEY_READ, &hkey) != ERROR_SUCCESS) {
        log_msg("InprocServer32 key not found under HKCU");
        return FALSE;
    }

    if (RegQueryValueExW(hkey, NULL, NULL, &type, (BYTE *)registered, &size) == ERROR_SUCCESS &&
        type == REG_SZ && registered[0]) {
        registered[G_N_ELEMENTS(registered) - 1] = L'\0';
        ok = same_path_ci(registered, dll_path);
        if (!ok)
            log_msg("DLL path mismatch: registered=[%ls] expected=[%ls]", registered, dll_path);
    }

    RegCloseKey(hkey);
    return ok;
}

static gboolean tsf_profiles_registered(void) {
    HKEY hkey = NULL;
    wchar_t key[MAX_PATH];
    gboolean found = FALSE;

    /* Check English profile (0x0409) */
    swprintf(key, MAX_PATH, L"%ls\\%ls", GTV_TSF_PROFILE_BASE, GTV_TSF_PROFILE_EN);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        RegCloseKey(hkey);
        found = TRUE;
    }

    /* Check Vietnamese profile (0x042A) */
    if (!found) {
        swprintf(key, MAX_PATH, L"%ls\\%ls", GTV_TSF_PROFILE_BASE, GTV_TSF_PROFILE_VI);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
            RegCloseKey(hkey);
            found = TRUE;
        }
    }

    if (!found)
        log_msg("No TSF language profiles found under HKCU");

    return found;
}

/* Manually register a TSF language profile under HKCU.
 * The ITfInputProcessorProfiles API may write to HKLM, which fails
 * for per-user installs.  We create the registry entries directly. */
static void ensure_profile_registered(const wchar_t *langid_path,
                                      const wchar_t *profile_guid,
                                      const wchar_t *description) {
    HKEY hKeyLang = NULL;
    HKEY hKeyProfile = NULL;
    LONG rc;

    /* Create ...\LanguageProfile\{langid} */
    rc = RegCreateKeyExW(HKEY_CURRENT_USER, GTV_TSF_PROFILE_BASE,
                         0, NULL, 0, KEY_WRITE, NULL, &hKeyLang, NULL);
    if (rc != ERROR_SUCCESS) {
        log_msg("Failed to create LanguageProfile key: %ld", (long)rc);
        return;
    }

    rc = RegCreateKeyExW(hKeyLang, langid_path,
                         0, NULL, 0, KEY_WRITE, NULL, &hKeyProfile, NULL);
    if (rc != ERROR_SUCCESS) {
        log_msg("Failed to create profile subkey %ls: %ld", langid_path, (long)rc);
        RegCloseKey(hKeyLang);
        return;
    }

    /* Set default value = description */
    rc = RegSetValueExW(hKeyProfile, NULL, 0, REG_SZ,
                        (const BYTE *)description,
                        (DWORD)((wcslen(description) + 1) * sizeof(wchar_t)));
    if (rc != ERROR_SUCCESS)
        log_msg("Failed to set profile description: %ld", (long)rc);

    /* Set Description = REG_SZ */
    rc = RegSetValueExW(hKeyProfile, L"Description", 0, REG_SZ,
                        (const BYTE *)description,
                        (DWORD)((wcslen(description) + 1) * sizeof(wchar_t)));

    RegCloseKey(hKeyProfile);
    RegCloseKey(hKeyLang);

    log_msg("Profile registered: %ls\\%ls", langid_path, profile_guid);
}

static void ensure_profiles_registered(void) {
    ensure_profile_registered(GTV_TSF_PROFILE_EN, NULL, L"GoTV");
    ensure_profile_registered(GTV_TSF_PROFILE_VI, NULL, L"GoTV");
}

/* Ensure the keyboard category (GUID_TFCAT_TIP_KEYBOARD) is registered
 * under HKCU.  The ITfCategoryMgr API may fail for per-user installs
 * because it tries to write to HKLM.  We write the key manually. */
static void ensure_category_registered(void) {
    HKEY hKeyCat = NULL;
    HKEY hKeyCatKB = NULL;
    LONG rc;

    /* Create ...Category base key */
    rc = RegCreateKeyExW(HKEY_CURRENT_USER, GTV_TSF_CATEGORY_BASE,
                         0, NULL, 0, KEY_WRITE, NULL, &hKeyCat, NULL);
    if (rc != ERROR_SUCCESS) {
        log_msg("Failed to create Category key: %ld", (long)rc);
        return;
    }

    /* Create ...Category\{GUID_TFCAT_TIP_KEYBOARD} subkey */
    rc = RegCreateKeyExW(hKeyCat, L"{34745C63-B2F0-11D0-98EF-00AA006D2E36}",
                         0, NULL, 0, KEY_WRITE, NULL, &hKeyCatKB, NULL);
    if (rc != ERROR_SUCCESS) {
        log_msg("Failed to create keyboard category subkey: %ld", (long)rc);
        RegCloseKey(hKeyCat);
        return;
    }

    /* Set the default value to the TIP CLSID to register it as a keyboard TIP */
    const wchar_t *clsid = L"{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}";
    rc = RegSetValueExW(hKeyCatKB, NULL, 0, REG_SZ,
                        (const BYTE *)clsid,
                        (DWORD)((wcslen(clsid) + 1) * sizeof(wchar_t)));
    if (rc != ERROR_SUCCESS)
        log_msg("Failed to set keyboard category value: %ld", (long)rc);
    else
        log_msg("Keyboard category registered under HKCU");

    RegCloseKey(hKeyCatKB);
    RegCloseKey(hKeyCat);
}

static wchar_t *prepend_app_dir_to_path(const wchar_t *app_dir) {
    DWORD old_len = GetEnvironmentVariableW(L"PATH", NULL, 0);
    wchar_t *old_path = NULL;
    wchar_t *new_path = NULL;
    size_t new_len;

    if (old_len > 0) {
        old_path = g_new0(wchar_t, old_len + 1);
        GetEnvironmentVariableW(L"PATH", old_path, old_len + 1);
        new_len = wcslen(app_dir) + 1 + wcslen(old_path);
    } else {
        new_len = wcslen(app_dir);
    }

    new_path = g_new0(wchar_t, new_len + 1);
    if (old_path && old_path[0])
        swprintf(new_path, new_len + 1, L"%ls;%ls", app_dir, old_path);
    else
        swprintf(new_path, new_len + 1, L"%ls", app_dir);

    SetEnvironmentVariableW(L"PATH", new_path);
    g_free(new_path);
    return old_path;
}

static void restore_path(wchar_t *old_path) {
    if (old_path)
        SetEnvironmentVariableW(L"PATH", old_path);
    else
        SetEnvironmentVariableW(L"PATH", L"");
    g_free(old_path);
}

static gboolean app_paths(wchar_t *app_dir, wchar_t *dll_path) {
    wchar_t exe_path[MAX_PATH];
    wchar_t *slash = NULL;

    if (!GetModuleFileNameW(NULL, exe_path, MAX_PATH))
        return FALSE;

    wcscpy(app_dir, exe_path);
    slash = wcsrchr(app_dir, L'\\');
    if (!slash) return FALSE;
    *slash = L'\0';

    swprintf(dll_path, MAX_PATH, L"%ls\\gtv_tsf.dll", app_dir);
    if (GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES) {
        log_msg("DLL not found: %ls", dll_path);
        return FALSE;
    }
    return TRUE;
}

void gtv_tsf_install_ensure_registered(void) {
    wchar_t app_dir[MAX_PATH];
    wchar_t dll_path[MAX_PATH];
    wchar_t system_dir[MAX_PATH];
    wchar_t regsvr32_path[MAX_PATH];
    wchar_t command_line[MAX_PATH * 3];
    wchar_t *old_path = NULL;
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};

    log_msg("=== gtv_tsf_install_ensure_registered ===");

    if (!app_paths(app_dir, dll_path)) {
        log_msg("app_paths failed - DLL missing or cannot determine app dir");
        return;
    }

    log_msg("app_dir=%ls  dll_path=%ls", app_dir, dll_path);

    /* Re-register if: InprocServer32 path mismatch OR TSF profiles missing
     * OR keyboard category missing. */
    gboolean inproc_ok = registered_to(dll_path);
    gboolean profiles_ok = tsf_profiles_registered();

    if (inproc_ok && profiles_ok) {
        /* Quick check: is the keyboard category also present? */
        HKEY hkey = NULL;
        DWORD cbData = 0;
        wchar_t cat_key[MAX_PATH];
        swprintf(cat_key, MAX_PATH, L"%ls\\%ls", GTV_TSF_CATEGORY_BASE, GTV_TSF_CAT_KEYBOARD);
        if (RegQueryValueExW(HKEY_CURRENT_USER, cat_key, NULL, NULL, NULL, &cbData) == ERROR_SUCCESS
            && cbData > 0) {
            log_msg("All TSF registrations OK - skipping regsvr32");
            return;
        }
        log_msg("Keyboard category missing - will re-register");
    }

    ensure_profiles_registered();
    ensure_category_registered();

    /* Build regsvr32 command */
    if (GetSystemDirectoryW(system_dir, MAX_PATH))
        swprintf(regsvr32_path, MAX_PATH, L"%ls\\regsvr32.exe", system_dir);
    else
        wcscpy(regsvr32_path, L"regsvr32.exe");

    swprintf(command_line, G_N_ELEMENTS(command_line),
             L"\"%ls\" /s \"%ls\"", regsvr32_path, dll_path);

    log_msg("Running: %ls", command_line);

    old_path = prepend_app_dir_to_path(app_dir);
    si.cb = sizeof(si);
    if (CreateProcessW(NULL, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, app_dir, &si, &pi)) {
        DWORD wait_rc = WaitForSingleObject(pi.hProcess, 15000);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        log_msg("regsvr32 wait=%lu exit_code=%lu", (unsigned long)wait_rc, (unsigned long)exit_code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        /* Verify after registration */
        if (registered_to(dll_path) && tsf_profiles_registered())
            log_msg("TSF registration verified OK");
        else
            log_msg("WARNING: TSF registration may have failed - check logs");
    } else {
        log_msg("CreateProcessW failed: %lu", (unsigned long)GetLastError());
    }

    restore_path(old_path);
}
