#include "tsf_service.h"
#include "tsf_langbar.h"

CGtvTextService::CGtvTextService() :
    m_cRef(1),
    m_pThreadMgr(NULL),
    m_tfClientId(0),
    m_dwThreadMgrEventSinkCookie(TF_INVALID_COOKIE),
    m_dwKeyEventSinkCookie(TF_INVALID_COOKIE),
    m_pComposition(NULL),
    m_pEngine(NULL),
    m_pLangBarItem(NULL),
    m_fEnabled(TRUE)
{
    ReloadConfig();
    DllAddRef();
}

void CGtvTextService::ReloadConfig()
{
    GtvConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gtv_config_load(&cfg, config_dir);

    m_fEnabled = TRUE;
    gchar *path = g_build_filename(config_dir, "config", NULL);
    GKeyFile *kf = g_key_file_new();
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
        if (g_key_file_has_key(kf, "input", "enabled", NULL))
            m_fEnabled = g_key_file_get_boolean(kf, "input", "enabled", NULL);
    }
    g_key_file_unref(kf);
    g_free(path);
    g_free(config_dir);

    if (m_pEngine) {
        m_pEngine->mode = cfg.mode;
        m_pEngine->modern = cfg.modern;
        m_pEngine->spellcheck = cfg.spellcheck;
    } else {
        m_pEngine = gtv_engine_new(&cfg);
    }
    gtv_config_clear(&cfg);
}

void CGtvTextService::SetEnabled(BOOL enabled)
{
    m_fEnabled = enabled;
    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gchar *path = g_build_filename(config_dir, "config", NULL);
    GKeyFile *kf = g_key_file_new();
    g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
    g_key_file_set_boolean(kf, "input", "enabled", m_fEnabled);
    gchar *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    g_key_file_save_to_file(kf, path, NULL);
    g_key_file_unref(kf);
    g_free(path);
    g_free(config_dir);
}

void CGtvTextService::ToggleEnabled()
{
    SetEnabled(!m_fEnabled);
}

CGtvTextService::~CGtvTextService()
{
    if (m_pEngine) {
        gtv_engine_free(m_pEngine);
        m_pEngine = NULL;
    }
    DllRelease();
}

// IUnknown implementation
STDMETHODIMP CGtvTextService::QueryInterface(REFIID riid, void **ppvObj)
{
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = NULL;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppvObj = static_cast<ITfTextInputProcessor*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CGtvTextService::AddRef(void)
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CGtvTextService::Release(void)
{
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}

// ITfTextInputProcessor
STDMETHODIMP CGtvTextService::Activate(ITfThreadMgr *ptim, TfClientId tid)
{
    if (!ptim) return E_INVALIDARG;
    m_pThreadMgr = ptim;
    m_pThreadMgr->AddRef();
    m_tfClientId = tid;

    if (!InitThreadMgrEventSink()) {
        Deactivate();
        return E_FAIL;
    }

    if (!InitKeyEventSink()) {
        Deactivate();
        return E_FAIL;
    }

    InitLangBarItem();

    return S_OK;
}

STDMETHODIMP CGtvTextService::Deactivate(void)
{
    /* NULL the member first: EndComposition re-enters through
     * OnCompositionTerminated, which would otherwise release the same
     * pointer again. */
    ITfComposition *pComp = m_pComposition;
    m_pComposition = NULL;
    if (pComp) {
        pComp->EndComposition(0);
        pComp->Release();
    }

    UninitLangBarItem();
    UninitKeyEventSink();
    UninitThreadMgrEventSink();

    if (m_pThreadMgr) {
        m_pThreadMgr->Release();
        m_pThreadMgr = NULL;
    }
    m_tfClientId = 0;

    if (m_pEngine) {
        gtv_engine_reset(m_pEngine);
    }

    return S_OK;
}

// ITfThreadMgrEventSink
STDMETHODIMP CGtvTextService::OnInitDocumentMgr(ITfDocumentMgr *pdim) { return S_OK; }
STDMETHODIMP CGtvTextService::OnUninitDocumentMgr(ITfDocumentMgr *pdim) { return S_OK; }
STDMETHODIMP CGtvTextService::OnSetFocus(ITfDocumentMgr *pdimFocus, ITfDocumentMgr *pdimPrevFocus)
{
    ITfComposition *pComp = m_pComposition;
    m_pComposition = NULL;
    if (pComp) {
        pComp->EndComposition(0);
        pComp->Release();
    }
    ReloadConfig();
    if (m_pEngine) {
        gtv_engine_reset(m_pEngine);
    }
    return S_OK;
}
STDMETHODIMP CGtvTextService::OnPushContext(ITfContext *pic) { return S_OK; }
STDMETHODIMP CGtvTextService::OnPopContext(ITfContext *pic) { return S_OK; }

// ITfCompositionSink
STDMETHODIMP CGtvTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition)
{
    if (m_pComposition) {
        m_pComposition->Release();
        m_pComposition = NULL;
    }
    if (m_pEngine) {
        gtv_engine_reset(m_pEngine);
    }
    return S_OK;
}

// Event Sink registrations
BOOL CGtvTextService::InitThreadMgrEventSink()
{
    ITfSource *pSource = NULL;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource)))
        return FALSE;

    HRESULT hr = pSource->AdviseSink(IID_ITfThreadMgrEventSink,
        static_cast<ITfThreadMgrEventSink*>(this),
        &m_dwThreadMgrEventSinkCookie);
    pSource->Release();
    return SUCCEEDED(hr);
}

void CGtvTextService::UninitThreadMgrEventSink()
{
    if (m_dwThreadMgrEventSinkCookie == TF_INVALID_COOKIE) return;
    if (!m_pThreadMgr) { m_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE; return; }
    ITfSource *pSource = NULL;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource))) {
        pSource->UnadviseSink(m_dwThreadMgrEventSinkCookie);
        pSource->Release();
    }
    m_dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
}

BOOL CGtvTextService::InitKeyEventSink()
{
    ITfKeystrokeMgr *pKeystrokeMgr = NULL;
    if (FAILED(m_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr)))
        return FALSE;

    HRESULT hr = pKeystrokeMgr->AdviseKeyEventSink(m_tfClientId,
        static_cast<ITfKeyEventSink*>(this), TRUE);
    pKeystrokeMgr->Release();
    return SUCCEEDED(hr);
}

void CGtvTextService::UninitKeyEventSink()
{
    if (!m_pThreadMgr) return;
    ITfKeystrokeMgr *pKeystrokeMgr = NULL;
    if (SUCCEEDED(m_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
        pKeystrokeMgr->UnadviseKeyEventSink(m_tfClientId);
        pKeystrokeMgr->Release();
    }
}

BOOL CGtvTextService::InitLangBarItem()
{
    if (m_pLangBarItem) return TRUE;

    ITfLangBarItemMgr *pLangBarItemMgr = NULL;
    HRESULT hr = m_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pLangBarItemMgr);
    if (FAILED(hr) || !pLangBarItemMgr) {
        typedef HRESULT (WINAPI *pfnTF_CreateLangBarItemMgr)(ITfLangBarItemMgr **pplbim);
        HMODULE hMsctf = GetModuleHandleA("msctf.dll");
        if (hMsctf) {
            pfnTF_CreateLangBarItemMgr pfn = reinterpret_cast<pfnTF_CreateLangBarItemMgr>(
                reinterpret_cast<void*>(GetProcAddress(hMsctf, "TF_CreateLangBarItemMgr")));
            if (pfn) pfn(&pLangBarItemMgr);
        }
    }
    if (!pLangBarItemMgr) return FALSE;

    m_pLangBarItem = new CGtvLangBarItem(this);
    hr = pLangBarItemMgr->AddItem(m_pLangBarItem);
    pLangBarItemMgr->Release();

    return SUCCEEDED(hr);
}

void CGtvTextService::UninitLangBarItem()
{
    if (!m_pLangBarItem) return;

    ITfLangBarItemMgr *pLangBarItemMgr = NULL;
    HRESULT hr = m_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pLangBarItemMgr);
    if (FAILED(hr) || !pLangBarItemMgr) {
        typedef HRESULT (WINAPI *pfnTF_CreateLangBarItemMgr)(ITfLangBarItemMgr **pplbim);
        HMODULE hMsctf = GetModuleHandleA("msctf.dll");
        if (hMsctf) {
            pfnTF_CreateLangBarItemMgr pfn = reinterpret_cast<pfnTF_CreateLangBarItemMgr>(
                reinterpret_cast<void*>(GetProcAddress(hMsctf, "TF_CreateLangBarItemMgr")));
            if (pfn) pfn(&pLangBarItemMgr);
        }
    }
    if (pLangBarItemMgr) {
        pLangBarItemMgr->RemoveItem(m_pLangBarItem);
        pLangBarItemMgr->Release();
    }

    m_pLangBarItem->Release();
    m_pLangBarItem = NULL;
}
