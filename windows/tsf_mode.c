#include "tsf_mode.h"
#include "tsf/tsf_defs.h"
#include <ole2.h>

gboolean gtv_tsf_set_enabled(gboolean on) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    gboolean need_uninit = SUCCEEDED(hr);
    gboolean ok = FALSE;
    ITfInputProcessorProfiles *p = NULL;
    /* NOTE: this is C, not C++: COM methods go through lpVtbl. */
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
            &IID_ITfInputProcessorProfiles, (void**)&p)) && p) {
        HRESULT r1 = p->lpVtbl->EnableLanguageProfile(p, &CLSID_GtvTextService,
            GTV_LANG_ENGLISH, &GUID_GtvProfile, on ? TRUE : FALSE);
        HRESULT r2 = p->lpVtbl->EnableLanguageProfile(p, &CLSID_GtvTextService,
            GTV_LANG_VIETNAMESE, &GUID_GtvProfile, on ? TRUE : FALSE);
        ok = SUCCEEDED(r1) && SUCCEEDED(r2);
        p->lpVtbl->Release(p);
    }
    if (need_uninit) CoUninitialize();
    return ok;
}
