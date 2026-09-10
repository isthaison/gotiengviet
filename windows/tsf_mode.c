#include "tsf_mode.h"
#include "tsf/tsf_defs.h"
#include <ole2.h>

gboolean gtv_tsf_set_enabled(gboolean on) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    gboolean need_uninit = SUCCEEDED(hr);
    gboolean ok = FALSE;
    ITfInputProcessorProfiles *p = NULL;
    /* tsf_defs.h (initguid) instantiates the GUIDs in this binary, which
     * is separate from gtv_tsf.dll, so there is no duplicate-symbol clash. */
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
            &IID_ITfInputProcessorProfiles, (void**)&p)) && p) {
        HRESULT r1 = p->EnableLanguageProfile(&CLSID_GtvTextService,
            GTV_LANG_ENGLISH, &GUID_GtvProfile, on ? TRUE : FALSE);
        HRESULT r2 = p->EnableLanguageProfile(&CLSID_GtvTextService,
            GTV_LANG_VIETNAMESE, &GUID_GtvProfile, on ? TRUE : FALSE);
        ok = SUCCEEDED(r1) && SUCCEEDED(r2);
        p->Release();
    }
    if (need_uninit) CoUninitialize();
    return ok;
}
