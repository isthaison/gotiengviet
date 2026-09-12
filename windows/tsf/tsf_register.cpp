#include <windows.h>
#include <ole2.h>
#include <msctf.h>
#include <new>
#include "tsf_defs.h"
#include "tsf_service.h"

static HINSTANCE g_hModule = NULL;
static volatile LONG g_cServerLocks = 0;
static volatile LONG g_cRefDll = 0;

/* DllCanUnloadNow must account for live objects, not just server locks. */
void DllAddRef(void) { InterlockedIncrement(&g_cRefDll); }
void DllRelease(void) { InterlockedDecrement(&g_cRefDll); }

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

class CClassFactory : public IClassFactory {
public:
    CClassFactory() : m_cRef(1) { DllAddRef(); }
    virtual ~CClassFactory() { DllRelease(); }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) {
        if (!ppvObj) return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppvObj = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObj = NULL;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        LONG c = InterlockedDecrement(&m_cRef);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj) {
        if (pUnkOuter != NULL) return CLASS_E_NOAGGREGATION;
        CGtvTextService *pService = new (std::nothrow) CGtvTextService();
        if (!pService) return E_OUTOFMEMORY;
        HRESULT hr = pService->QueryInterface(riid, ppvObj);
        pService->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) {
        if (fLock) InterlockedIncrement(&g_cServerLocks);
        else InterlockedDecrement(&g_cServerLocks);
        return S_OK;
    }

private:
    volatile LONG m_cRef;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    if (!ppv) return E_INVALIDARG;
    *ppv = NULL;

    if (!IsEqualCLSID(rclsid, CLSID_GtvTextService))
        return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory *pFactory = new (std::nothrow) CClassFactory();
    if (!pFactory) return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void)
{
    return (g_cServerLocks == 0 && g_cRefDll == 0) ? S_OK : S_FALSE;
}

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
    if (!GetModuleFileNameW(g_hModule, szModule, MAX_PATH))
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

        pProfiles->AddLanguageProfile(CLSID_GtvTextService,
            GTV_LANG_VIETNAMESE,
            GUID_GtvProfile,
            GTV_TSF_DESC,
            (ULONG)wcslen(GTV_TSF_DESC),
            szModule,
            (ULONG)wcslen(szModule),
            0);

        pProfiles->AddLanguageProfile(CLSID_GtvTextService,
            GTV_LANG_ENGLISH,
            GUID_GtvProfile,
            GTV_TSF_DESC,
            (ULONG)wcslen(GTV_TSF_DESC),
            szModule,
            (ULONG)wcslen(szModule),
            0);

        pProfiles->Release();
    }

    /* Register TSF Categories: keyboard TIP must register BOTH
     * GUID_TFCAT_TIP_TEXTSERVICE (general text service) and
     * GUID_TFCAT_TIP_KEYBOARD (keyboard-specific). Without TEXTSERVICE,
     * Windows 10 will not list the keyboard in language settings.
     * Same per-user caveat applies — TSF APIs may fail on HKLM write. */
    ITfCategoryMgr *pCategoryMgr = NULL;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr);

    if (SUCCEEDED(hr) && pCategoryMgr) {
        pCategoryMgr->RegisterCategory(
            CLSID_GtvTextService, GUID_TFCAT_TIP_TEXTSERVICE, CLSID_GtvTextService);
        pCategoryMgr->RegisterCategory(
            CLSID_GtvTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GtvTextService);
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
        pCategoryMgr->UnregisterCategory(CLSID_GtvTextService, GUID_TFCAT_TIP_TEXTSERVICE, CLSID_GtvTextService);
        pCategoryMgr->UnregisterCategory(CLSID_GtvTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GtvTextService);
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
