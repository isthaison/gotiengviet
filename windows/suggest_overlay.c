#include "suggest_overlay.h"
#include "suggest_ipc.h"

#include <string.h>
#include <wchar.h>

#define GTV_SUGGEST_OVERLAY_CLASS L"GoTiengVietSuggestionOverlay"

typedef struct {
    HWND hwnd;
    HFONT font;
    DWORD source_pid;
    DWORD generation;
    DWORD count;
    DWORD selected;
    WCHAR candidates[GTV_SUGGEST_IPC_MAX_CANDIDATES]
                    [GTV_SUGGEST_IPC_CANDIDATE_BYTES];
} GtvSuggestOverlay;

static GtvSuggestOverlay s_overlay;

static HFONT overlay_font(void) {
    if (!s_overlay.font) {
        HDC dc = GetDC(NULL);
        int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
        if (dc) ReleaseDC(NULL, dc);
        s_overlay.font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL,
                                     FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Segoe UI");
    }
    return s_overlay.font;
}

static int overlay_row_height(HDC dc) {
    TEXTMETRICW metrics;
    HFONT font = overlay_font();
    HFONT old = font ? (HFONT)SelectObject(dc, font) : NULL;
    int height = GetTextMetricsW(dc, &metrics) ? metrics.tmHeight + 8 : 24;
    if (old) SelectObject(dc, old);
    return height;
}

static void overlay_measure(int *width, int *height) {
    HDC dc = GetDC(s_overlay.hwnd);
    int max_width = 140;
    int row_height = 24;
    if (dc) {
        row_height = overlay_row_height(dc);
        HFONT font = overlay_font();
        HFONT old = font ? (HFONT)SelectObject(dc, font) : NULL;
        for (DWORD i = 0; i < s_overlay.count; i++) {
            SIZE size;
            if (GetTextExtentPoint32W(dc, s_overlay.candidates[i],
                                      (int)wcslen(s_overlay.candidates[i]), &size) &&
                size.cx + 36 > max_width)
                max_width = size.cx + 36;
        }
        if (old) SelectObject(dc, old);
        ReleaseDC(s_overlay.hwnd, dc);
    }
    *width = max_width > 340 ? 340 : max_width;
    *height = (int)s_overlay.count * row_height + 4;
}

static void overlay_show(RECT caret) {
    int width, height;
    overlay_measure(&width, &height);

    HMONITOR monitor = MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    RECT work = caret;
    if (monitor && GetMonitorInfoW(monitor, &info)) work = info.rcWork;

    int x = caret.left;
    int y = caret.bottom + 2;
    if (y + height > work.bottom) y = caret.top - height - 2;
    if (x + width > work.right) x = work.right - width;
    if (x < work.left) x = work.left;
    if (y < work.top) y = work.top;

    SetWindowPos(s_overlay.hwnd, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(s_overlay.hwnd, NULL, TRUE);
}

static void overlay_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client;
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, (HBRUSH)(COLOR_WINDOW + 1));
    FrameRect(dc, &client, (HBRUSH)(COLOR_WINDOWFRAME + 1));

    int row_height = overlay_row_height(dc);
    HFONT font = overlay_font();
    HFONT old = font ? (HFONT)SelectObject(dc, font) : NULL;
    SetBkMode(dc, TRANSPARENT);
    for (DWORD i = 0; i < s_overlay.count; i++) {
        WCHAR label[GTV_SUGGEST_IPC_CANDIDATE_BYTES + 8];
        RECT row = { 1, 2 + (int)i * row_height, client.right - 1,
                     2 + ((int)i + 1) * row_height };
        if (i == s_overlay.selected) {
            FillRect(dc, &row, (HBRUSH)(COLOR_HIGHLIGHT + 1));
            SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        }
        swprintf(label, sizeof(label) / sizeof(label[0]), L"%lu  %ls",
                 (unsigned long)(i + 1), s_overlay.candidates[i]);
        label[(sizeof(label) / sizeof(label[0])) - 1] = L'\0';
        row.left += 6;
        DrawTextW(dc, label, -1, &row,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    if (old) SelectObject(dc, old);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK overlay_wndproc(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam) {
    (void)wparam;
    (void)lparam;
    switch (message) {
        case WM_PAINT:
            overlay_paint(hwnd);
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

BOOL gtv_suggest_overlay_init(HINSTANCE instance, HWND owner) {
    WNDCLASSW window_class = {0};
    window_class.lpfnWndProc = overlay_wndproc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = GTV_SUGGEST_OVERLAY_CLASS;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;

    s_overlay.hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                     GTV_SUGGEST_OVERLAY_CLASS, L"GoTiengViet",
                                     WS_POPUP, 0, 0, 1, 1, owner, NULL, instance, NULL);
    return s_overlay.hwnd != NULL;
}

void gtv_suggest_overlay_cleanup(void) {
    if (s_overlay.hwnd && IsWindow(s_overlay.hwnd)) DestroyWindow(s_overlay.hwnd);
    s_overlay.hwnd = NULL;
    if (s_overlay.font) DeleteObject(s_overlay.font);
    s_overlay.font = NULL;
    s_overlay.source_pid = 0;
    s_overlay.generation = 0;
    s_overlay.count = 0;
    s_overlay.selected = 0;
}

static BOOL overlay_decode_candidate(const char *input, WCHAR *output) {
    const char *nul = (const char *)memchr(input, '\0', GTV_SUGGEST_IPC_CANDIDATE_BYTES);
    if (!nul || nul == input) return FALSE;
    int bytes = (int)(nul - input) + 1;
    int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, bytes,
                                    output, GTV_SUGGEST_IPC_CANDIDATE_BYTES);
    return chars > 0;
}

BOOL gtv_suggest_overlay_handle_copydata(const COPYDATASTRUCT *copydata) {
    if (!s_overlay.hwnd || !copydata || copydata->dwData != GTV_SUGGEST_COPYDATA_ID ||
        copydata->cbData != sizeof(GtvSuggestIpcPayload) || !copydata->lpData)
        return FALSE;

    const GtvSuggestIpcPayload *payload = (const GtvSuggestIpcPayload *)copydata->lpData;
    if (payload->version != GTV_SUGGEST_IPC_VERSION || !payload->source_pid ||
        !payload->generation || payload->candidate_count > GTV_SUGGEST_IPC_MAX_CANDIDATES)
        return FALSE;

    if (payload->command == GTV_SUGGEST_IPC_HIDE) {
        if (payload->candidate_count != 0 || payload->source_pid != s_overlay.source_pid ||
            payload->generation < s_overlay.generation)
            return FALSE;
        s_overlay.generation = payload->generation;
        s_overlay.count = 0;
        ShowWindow(s_overlay.hwnd, SW_HIDE);
        return TRUE;
    }
    if (payload->command != GTV_SUGGEST_IPC_SHOW || payload->candidate_count == 0)
        return FALSE;
    if (payload->source_pid == s_overlay.source_pid &&
        payload->generation < s_overlay.generation)
        return FALSE;

    WCHAR candidates[GTV_SUGGEST_IPC_MAX_CANDIDATES][GTV_SUGGEST_IPC_CANDIDATE_BYTES];
    for (DWORD i = 0; i < payload->candidate_count; i++) {
        if (!overlay_decode_candidate(payload->candidates[i], candidates[i])) return FALSE;
    }

    s_overlay.source_pid = payload->source_pid;
    s_overlay.generation = payload->generation;
    s_overlay.count = payload->candidate_count;
    s_overlay.selected = payload->selected < payload->candidate_count
                         ? payload->selected : 0;
    memcpy(s_overlay.candidates, candidates, sizeof(candidates));
    overlay_show(payload->caret_screen);
    return TRUE;
}
