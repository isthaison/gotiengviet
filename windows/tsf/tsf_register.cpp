#include <windows.h>
#include <ole2.h>
#include <msctf.h>
#include "tsf_defs.h"

/* Registration entry points for the STABLE stub (gtv_tsf.dll).
 * Linked into the stub only: the registered InprocServer32 path is the
 * stub itself, so it never changes and updates never re-register.
 * DllGetClassObject/DllCanUnloadNow live in tsf_factory.cpp (engine DLL);
 * the stub forwards to them. */

#ifdef __cplusplus
extern "C" HINSTANCE gtv_stub_instance(void);
#else
HINSTANCE gtv_stub_instance(void);
#endif

/* Per-user COM registration under HKCU\Software\Classes: the installer
 * runs without elevation (PrivilegesRequired=lowest), so HKLM writes
 * would fail silently and the service would never activate. HKCU classes
 * take precedence over HKLM for COM lookup. */
static const WCHAR *const kClassesRoot = L"Software\\Classes\\CLSID\\";

static BOOL RegisterServerKeys(LPCWSTR szClsid, LPCWSTR szModule)
{
    WCHAR szKey[256];
    HKEY hKey;
    BOOL ok = TRUE;

    wsprintfW(szKey, L"%ls%ls", kClassesRoot, szClsid);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, szKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return FALSE;
    if (RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)GTV_TSF_MODEL_NAME, (DWORD)((wcslen(GTV_TSF_MODEL_NAME) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS)
        ok = FALSE;
    RegCloseKey(hKey);

    wsprintfW(szKey, L"%ls%ls\\InprocServer32", kClassesRoot, szClsid);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, szKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return FALSE;
    if (RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)szModule, (DWORD)((wcslen(szModule) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS)
        ok = FALSE;
    else {
        LPCWSTR szModel = L"Apartment";
        if (RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)szModel, (DWORD)((wcslen(szModel) + 1) * sizeof(WCHAR))) != ERROR_SUCCESS)
            ok = FALSE;
    }
    RegCloseKey(hKey);
    return ok;
}

static void UnregisterServerKeys(LPCWSTR szClsid)
{
    WCHAR szKey[256];
    wsprintfW(szKey, L"%ls%ls\\InprocServer32", kClassesRoot, szClsid);
    RegDeleteKeyW(HKEY_CURRENT_USER, szKey);
    wsprintfW(szKey, L"%ls%ls", kClassesRoot, szClsid);
    RegDeleteKeyW(HKEY_CURRENT_USER, szKey);
}

STDAPI DllRegisterServer(void)
{
    WCHAR szModule[MAX_PATH];
    if (!GetModuleFileNameW(gtv_stub_instance(), szModule, MAX_PATH))
        return HRESULT_FROM_WIN32(GetLastError());

    WCHAR szClsid[64];
    if (!StringFromGUID2(CLSID_GtvTextService, szClsid, 64))
        return E_FAIL;

    /* Register COM class under HKCU (per-user). */
    if (!RegisterServerKeys(szClsid, szModule))
        return E_FAIL;

    /* Register TSF Profiles — these APIs may try to write to HKLM.
     * For per-user installs, failures are expected; the actual
     * per-user TSF profile/category registration is handled by
     * gtv_tsf_install_ensure_registered() in tsf_install.c using
     * direct registry writes under HKCU.  We continue even if the
     * TSF APIs fail so that the COM class is still usable. */
    ITfInputProcessorProfiles *pProfiles = NULL;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, (void**)&pProfiles);

    if (SUCCEEDED(hr) && pProfiles) {
        pProfiles->Register(CLSID_GtvTextService);

        /* Register ONLY under Vietnamese (0x042A). Do NOT register under
         * en-US — that causes duplicate keyboard entries in Settings/Win+Space.
         * Users add Vietnamese language in Settings to get the GoTV keyboard. */
        pProfiles->AddLanguageProfile(CLSID_GtvTextService,
            GTV_LANG_VIETNAMESE,
            GUID_GtvProfile,
            GTV_TSF_DESC,
            (ULONG)wcslen(GTV_TSF_DESC),
            szModule,
            (ULONG)wcslen(szModule),
            0);
        pProfiles->EnableLanguageProfile(CLSID_GtvTextService, GTV_LANG_VIETNAMESE, GUID_GtvProfile, TRUE);

        pProfiles->Release();
    }

    /* Register TSF Categories: keyboard TIP must register
     * GUID_TFCAT_TIP_KEYBOARD, GUID_TFCAT_TIP_TEXTSERVICE, and the
     * {34745C63-B2F0-4784-8B67-5E12C8701A31} keyboard category that
     * Windows' own TIPs carry (HKLM\SOFTWARE\Microsoft\CTF\TIP) —
     * without it the keyboard never appears in language settings.
     * Same per-user caveat applies — TSF APIs may fail on HKLM write. */
    ITfCategoryMgr *pCategoryMgr = NULL;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr);

    if (SUCCEEDED(hr) && pCategoryMgr) {
        GUID guidCat4784 = GUID_NULL;
        CLSIDFromString(L"{34745C63-B2F0-4784-8B67-5E12C8701A31}", &guidCat4784);
        pCategoryMgr->RegisterCategory(
            CLSID_GtvTextService, GUID_TFCAT_TIP_TEXTSERVICE, CLSID_GtvTextService);
        pCategoryMgr->RegisterCategory(
            CLSID_GtvTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GtvTextService);
        pCategoryMgr->RegisterCategory(
            CLSID_GtvTextService, guidCat4784, CLSID_GtvTextService);
        pCategoryMgr->Release();
    }

    /* Always succeed: even if TSF APIs failed (per-user HKLM write denied),
     * gtv_tsf_install_ensure_registered() will register profiles/categories
     * under HKCU directly when the app starts. */
    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    WCHAR szClsid[64];
    if (!StringFromGUID2(CLSID_GtvTextService, szClsid, 64))
        return E_FAIL;

    /* Unregister TSF Categories — ignore failures (may not have HKLM access). */
    ITfCategoryMgr *pCategoryMgr = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr)) && pCategoryMgr) {
        GUID guidCat4784 = GUID_NULL;
        CLSIDFromString(L"{34745C63-B2F0-4784-8B67-5E12C8701A31}", &guidCat4784);
        pCategoryMgr->UnregisterCategory(CLSID_GtvTextService, GUID_TFCAT_TIP_TEXTSERVICE, CLSID_GtvTextService);
        pCategoryMgr->UnregisterCategory(CLSID_GtvTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GtvTextService);
        pCategoryMgr->UnregisterCategory(CLSID_GtvTextService, guidCat4784, CLSID_GtvTextService);
        pCategoryMgr->Release();
    }

    /* Unregister TSF Profiles — ignore failures. */
    ITfInputProcessorProfiles *pProfiles = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, (void**)&pProfiles)) && pProfiles) {
        pProfiles->Unregister(CLSID_GtvTextService);
        pProfiles->Release();
    }

    UnregisterServerKeys(szClsid);
    return S_OK;
}
