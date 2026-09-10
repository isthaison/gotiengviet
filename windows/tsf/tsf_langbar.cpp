#include "tsf_langbar.h"
#include "tsf_service.h"

CGtvLangBarItem::CGtvLangBarItem(CGtvTextService *pService) :
    m_cRef(1),
    m_pService(pService),
    m_hIcon(NULL)
{
    m_hIcon = CreateGoTvIcon();
}

CGtvLangBarItem::~CGtvLangBarItem()
{
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
        m_hIcon = NULL;
    }
}

STDMETHODIMP CGtvLangBarItem::QueryInterface(REFIID riid, void **ppvObj)
{
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = NULL;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfLangBarItem) ||
        IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppvObj = static_cast<ITfLangBarItemButton*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CGtvLangBarItem::AddRef(void)
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CGtvLangBarItem::Release(void)
{
    LONG c = InterlockedDecrement(&m_cRef);
    if (c == 0) delete this;
    return c;
}

STDMETHODIMP CGtvLangBarItem::GetInfo(TF_LANGBARITEMINFO *pInfo)
{
    if (!pInfo) return E_INVALIDARG;
    pInfo->clsidService = CLSID_GtvTextService;
    pInfo->guidItem = GUID_GtvLangBarItem;
    pInfo->dwStyle = TF_LBI_STYLE_SHOWNINTRAY | TF_LBI_STYLE_BTN_BUTTON;
    pInfo->ulSort = 0;
    wcsncpy(pInfo->szDescription, L"GoTV", 32);
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::GetStatus(DWORD *pdwStatus)
{
    if (!pdwStatus) return E_INVALIDARG;
    *pdwStatus = 0;
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::Show(BOOL fShow)
{
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::GetTooltipString(BSTR *pbstrToolTip)
{
    if (!pbstrToolTip) return E_INVALIDARG;
    *pbstrToolTip = SysAllocString(L"GoTiengViet (GoTV)");
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::GetText(BSTR *pbstrText)
{
    if (!pbstrText) return E_INVALIDARG;
    *pbstrText = SysAllocString(L"GoTV");
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::GetIcon(HICON *phIcon)
{
    if (!phIcon) return E_INVALIDARG;
    *phIcon = m_hIcon ? CopyIcon(m_hIcon) : NULL;
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::OnClick(TfLBIClick click, POINT pt, const RECT *prcArea)
{
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::InitMenu(ITfMenu *pMenu)
{
    return S_OK;
}

STDMETHODIMP CGtvLangBarItem::OnMenuSelect(UINT uID)
{
    return S_OK;
}

HICON CGtvLangBarItem::CreateGoTvIcon()
{
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);

    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmColor);

    // Dark red background with rounded feel
    RECT rc = { 0, 0, cx, cy };
    HBRUSH hbr = CreateSolidBrush(RGB(178, 34, 34)); // Firebrick red
    FillRect(hdcMem, &rc, hbr);
    DeleteObject(hbr);

    // Bold text "V" for Vietnamese input
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 255));
    HFONT hFont = CreateFontW(cy - 2, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT hFontOld = (HFONT)SelectObject(hdcMem, hFont);

    DrawTextW(hdcMem, L"V", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hFontOld);
    DeleteObject(hFont);
    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    ICONINFO ii;
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return hIcon;
}
