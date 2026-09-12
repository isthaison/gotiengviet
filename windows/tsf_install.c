#include "tsf_install.h"

#include <windows.h>
#include <glib.h>
#include <wchar.h>

static const wchar_t *GTV_TSF_INPROC_KEY =
    L"Software\\Classes\\CLSID\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\InprocServer32";

// TSF profile keys: HKCU\Software\Microsoft\CTF\TIP\{CLSID}\LanguageProfile\{langid}\{profileguid}
// If these don't exist, the keyboard won't appear in language settings.
static const wchar_t *GTV_TSF_PROFILE_BASE =
    L"Software\\Microsoft\\CTF\\TIP\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\LanguageProfile";
static const wchar_t *GTV_TSF_PROFILE_EN = L"0x00000409\\{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}";
static const wchar_t *GTV_TSF_PROFILE_VI = L"0x0000042a\\{D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D}";

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

    if (RegOpenKeyExW(HKEY_CURRENT_USER, GTV_TSF_INPROC_KEY, 0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return FALSE;

    if (RegQueryValueExW(hkey, NULL, NULL, &type, (BYTE *)registered, &size) == ERROR_SUCCESS &&
        type == REG_SZ && registered[0]) {
        registered[G_N_ELEMENTS(registered) - 1] = L'\0';
        ok = same_path_ci(registered, dll_path);
    }

    RegCloseKey(hkey);
    return ok;
}

static gboolean tsf_profiles_registered(void) {
    HKEY hkey = NULL;
    wchar_t key[MAX_PATH];
    gboolean found = FALSE;

    // Check English profile (0x0409) - should exist on most systems
    swprintf(key, MAX_PATH, L"%ls\\%ls", GTV_TSF_PROFILE_BASE, GTV_TSF_PROFILE_EN);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        RegCloseKey(hkey);
        found = TRUE;
    }

    // Also check Vietnamese profile (0x042A)
    if (!found) {
        swprintf(key, MAX_PATH, L"%ls\\%ls", GTV_TSF_PROFILE_BASE, GTV_TSF_PROFILE_VI);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
            RegCloseKey(hkey);
            found = TRUE;
        }
    }

    return found;
}

static wchar_t *prepend_app_dir_to_path(const wchar_t *app_dir) {
    DWORD old_len = GetEnvironmentVariableW(L"PATH", NULL, 0);
    wchar_t *old_path = NULL;
    wchar_t *new_path = NULL;
    size_t new_len = wcslen(app_dir) + 1;

    if (old_len > 0) {
        old_path = g_new0(wchar_t, old_len);
        GetEnvironmentVariableW(L"PATH", old_path, old_len);
        if (old_path[0]) new_len += 1 + wcslen(old_path);
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
    SetEnvironmentVariableW(L"PATH", old_path);
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
    return GetFileAttributesW(dll_path) != INVALID_FILE_ATTRIBUTES;
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

    if (!app_paths(app_dir, dll_path))
        return;

    // Re-register if: InprocServer32 path mismatch OR TSF profiles missing
    if (registered_to(dll_path) && tsf_profiles_registered())
        return;

    if (GetSystemDirectoryW(system_dir, MAX_PATH))
        swprintf(regsvr32_path, MAX_PATH, L"%ls\\regsvr32.exe", system_dir);
    else
        wcscpy(regsvr32_path, L"regsvr32.exe");

    swprintf(command_line, G_N_ELEMENTS(command_line),
             L"\"%ls\" /s \"%ls\"", regsvr32_path, dll_path);

    old_path = prepend_app_dir_to_path(app_dir);
    si.cb = sizeof(si);
    if (CreateProcessW(NULL, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, app_dir, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    restore_path(old_path);
}
