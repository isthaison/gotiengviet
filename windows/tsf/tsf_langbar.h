#ifndef GTV_TSF_LANGBAR_H
#define GTV_TSF_LANGBAR_H

#include <windows.h>
#include <ole2.h>
#include <msctf.h>
#include <ctfutb.h>
#include "tsf_defs.h"

// {7C12658A-7057-4E52-870B-51B8380E836D}
DEFINE_GUID(GUID_GtvLangBarItem,
    0x7c12658a, 0x7057, 0x4e52, 0x87, 0x0b, 0x51, 0xb8, 0x38, 0x0e, 0x83, 0x6d);

#ifndef __ITfLangBarItemButton_INTERFACE_DEFINED__
#define __ITfLangBarItemButton_INTERFACE_DEFINED__

interface ITfMenu;

DEFINE_GUID(IID_ITfLangBarItemButton,
    0x28c520fa, 0x7427, 0x41a5, 0xba, 0x98, 0xce, 0x0d, 0xa1, 0x44, 0x1f, 0x1d);

typedef enum {
    TF_LBI_CLK_RIGHT = 1,
    TF_LBI_CLK_LEFT  = 2
} TfLBIClick;

#define TF_LBI_STYLE_SHOWNINTRAY 0x00000002
#define TF_LBI_STYLE_BTN_BUTTON  0x00010000

MIDL_INTERFACE("28c520fa-7427-41a5-ba98-ce0da1441f1d")
ITfLangBarItemButton : public ITfLangBarItem
{
public:
    virtual HRESULT STDMETHODCALLTYPE OnClick(TfLBIClick click, POINT pt, const RECT *prcArea) = 0;
    virtual HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu *pMenu) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT uID) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIcon(HICON *phIcon) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetText(BSTR *pbstrText) = 0;
};
#endif

class CGtvTextService;

class CGtvLangBarItem : public ITfLangBarItemButton {
public:
    CGtvLangBarItem(CGtvTextService *pService);
    virtual ~CGtvLangBarItem();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    // ITfLangBarItem
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO *pInfo);
    STDMETHODIMP GetStatus(DWORD *pdwStatus);
    STDMETHODIMP Show(BOOL fShow);
    STDMETHODIMP GetTooltipString(BSTR *pbstrToolTip);

    // ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT *prcArea);
    STDMETHODIMP InitMenu(ITfMenu *pMenu);
    STDMETHODIMP OnMenuSelect(UINT uID);
    STDMETHODIMP GetIcon(HICON *phIcon);
    STDMETHODIMP GetText(BSTR *pbstrText);

private:
    LONG m_cRef;
    CGtvTextService *m_pService;
    HICON m_hIcon;

    HICON CreateGoTvIcon(BOOL enabled);
};

#endif // GTV_TSF_LANGBAR_H
