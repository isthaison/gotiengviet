#include <windows.h>
#include <ole2.h>
#include <msctf.h>
#include "tsf_defs.h"
#include "tsf_service.h"

static HINSTANCE g_hModule = NULL;
static LONG g_cServerLocks = 0;

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
    CClassFactory() : m_cRef(1) {}
    virtual ~CClassFactory() {}

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
        CGtvTextService *pService = new CGtvTextService();
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
    LONG m_cRef;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    if (!ppv) return E_INVALIDARG;
    *ppv = NULL;

    if (!IsEqualCLSID(rclsid, CLSID_GtvTextService))
        return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory *pFactory = new CClassFactory();
    if (!pFactory) return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void)
{
    return (g_cServerLocks == 0) ? S_OK : S_FALSE;
}

static BOOL RegisterServerKeys(LPCWSTR szClsid, LPCWSTR szModule)
{
    WCHAR szKey[256];
    wsprintfW(szKey, L"Software\\Classes\\CLSID\\%ls", szClsid);

    HKEY hKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, szKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)GTV_TSF_MODEL_NAME, (DWORD)((wcslen(GTV_TSF_MODEL_NAME) + 1) * sizeof(WCHAR)));
        RegCloseKey(hKey);

        wsprintfW(szKey, L"Software\\Classes\\CLSID\\%ls\\InprocServer32", szClsid);
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, szKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)szModule, (DWORD)((wcslen(szModule) + 1) * sizeof(WCHAR)));
            LPCWSTR szModel = L"Apartment";
            RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)szModel, (DWORD)((wcslen(szModel) + 1) * sizeof(WCHAR)));
            RegCloseKey(hKey);
        }
    }

    wsprintfW(szKey, L"CLSID\\%ls", szClsid);
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, szKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)GTV_TSF_MODEL_NAME, (DWORD)((wcslen(GTV_TSF_MODEL_NAME) + 1) * sizeof(WCHAR)));
        RegCloseKey(hKey);

        wsprintfW(szKey, L"CLSID\\%ls\\InprocServer32", szClsid);
        if (RegCreateKeyExW(HKEY_CLASSES_ROOT, szKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)szModule, (DWORD)((wcslen(szModule) + 1) * sizeof(WCHAR)));
            LPCWSTR szModel = L"Apartment";
            RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)szModel, (DWORD)((wcslen(szModel) + 1) * sizeof(WCHAR)));
            RegCloseKey(hKey);
        }
    }

    return TRUE;
}

static void UnregisterServerKeys(LPCWSTR szClsid)
{
    WCHAR szKey[256];
    wsprintfW(szKey, L"Software\\Classes\\CLSID\\%ls\\InprocServer32", szClsid);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, szKey);
    wsprintfW(szKey, L"Software\\Classes\\CLSID\\%ls", szClsid);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, szKey);

    wsprintfW(szKey, L"CLSID\\%ls\\InprocServer32", szClsid);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, szKey);
    wsprintfW(szKey, L"CLSID\\%ls", szClsid);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, szKey);
}

STDAPI DllRegisterServer(void)
{
    WCHAR szModule[MAX_PATH];
    if (!GetModuleFileNameW(g_hModule, szModule, MAX_PATH))
        return HRESULT_FROM_WIN32(GetLastError());

    WCHAR szClsid[64];
    if (!StringFromGUID2(CLSID_GtvTextService, szClsid, 64))
        return E_FAIL;

    if (!RegisterServerKeys(szClsid, szModule))
        return E_FAIL;

    // Register TSF Profiles
    ITfInputProcessorProfiles *pProfiles = NULL;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, (void**)&pProfiles);

    if (SUCCEEDED(hr) && pProfiles) {
        hr = pProfiles->Register(CLSID_GtvTextService);
        // Register under English (US) - so it appears in Win+Space on standard Windows
        pProfiles->AddLanguageProfile(CLSID_GtvTextService,
            GTV_LANG_ENGLISH,
            GUID_GtvProfile,
            GTV_TSF_DESC,
            (ULONG)wcslen(GTV_TSF_DESC),
            szModule,
            (ULONG)wcslen(szModule),
            0);
        pProfiles->EnableLanguageProfile(CLSID_GtvTextService, GTV_LANG_ENGLISH, GUID_GtvProfile, TRUE);

        // Also register under Vietnamese
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

    // Register TSF Categories
    ITfCategoryMgr *pCategoryMgr = NULL;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr);

    if (SUCCEEDED(hr) && pCategoryMgr) {
        pCategoryMgr->RegisterCategory(CLSID_GtvTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GtvTextService);
        pCategoryMgr->RegisterCategory(CLSID_GtvTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_GtvTextService);
        pCategoryMgr->Release();
    }

    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    WCHAR szClsid[64];
    if (!StringFromGUID2(CLSID_GtvTextService, szClsid, 64))
        return E_FAIL;

    // Unregister TSF Categories
    ITfCategoryMgr *pCategoryMgr = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (void**)&pCategoryMgr)) && pCategoryMgr) {
        pCategoryMgr->UnregisterCategory(CLSID_GtvTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_GtvTextService);
        pCategoryMgr->UnregisterCategory(CLSID_GtvTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_GtvTextService);
        pCategoryMgr->Release();
    }

    // Unregister TSF Profiles
    ITfInputProcessorProfiles *pProfiles = NULL;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, (void**)&pProfiles)) && pProfiles) {
        pProfiles->Unregister(CLSID_GtvTextService);
        pProfiles->Release();
    }

    UnregisterServerKeys(szClsid);
    return S_OK;
}
