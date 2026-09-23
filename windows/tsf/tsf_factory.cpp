#include <windows.h>
#include <ole2.h>
#include <msctf.h>
#include <new>
#include "tsf_defs.h"
#include "tsf_service.h"

/* Class factory + COM entry points for the versioned engine DLL
 * (gtv_engine.dll). The stable stub (gtv_tsf.dll) forwards
 * DllGetClassObject/DllCanUnloadNow here after LoadLibrary.
 * Registration (DllRegisterServer) lives in tsf_register.cpp, which is
 * linked into the stub only: the registered module path never changes. */

static volatile LONG g_cServerLocks = 0;
static volatile LONG g_cRefDll = 0;

/* DllCanUnloadNow must account for live objects, not just server locks. */
void DllAddRef(void) { InterlockedIncrement(&g_cRefDll); }
void DllRelease(void) { InterlockedDecrement(&g_cRefDll); }

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
    }
    (void)hinstDLL;
    (void)lpvReserved;
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
