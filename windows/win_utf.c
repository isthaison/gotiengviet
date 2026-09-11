#include "win_utf.h"

void gtv_win_copy_utf8(WCHAR *dst, guint dst_chars, const gchar *utf8) {
    guint i = 0;
    gunichar2 *src = utf8 ? g_utf8_to_utf16(utf8, -1, NULL, NULL, NULL) : NULL;

    if (src) {
        while (i + 1 < dst_chars && src[i]) {
            dst[i] = (WCHAR)src[i];
            i++;
        }
        if (i > 0 && i + 1 >= dst_chars && dst[i - 1] >= 0xD800 && dst[i - 1] <= 0xDBFF)
            i--;
    }
    dst[i] = 0;
    g_free(src);
}

void gtv_win_set_window_text(HWND hwnd, const gchar *utf8) {
    gunichar2 *w = g_utf8_to_utf16(utf8 ? utf8 : "", -1, NULL, NULL, NULL);
    if (w) {
        SetWindowTextW(hwnd, (LPCWSTR)w);
        g_free(w);
    }
}

void gtv_win_set_dlg_item_text(HWND hwnd, int id, const gchar *utf8) {
    gunichar2 *w = g_utf8_to_utf16(utf8 ? utf8 : "", -1, NULL, NULL, NULL);
    if (w) {
        SetDlgItemTextW(hwnd, id, (LPCWSTR)w);
        g_free(w);
    }
}

gchar *gtv_win_get_dlg_item_text(HWND hwnd, int id, int max_chars) {
    WCHAR *wbuf = g_new0(WCHAR, max_chars > 0 ? max_chars : 1);
    gchar *utf8 = NULL;

    GetDlgItemTextW(hwnd, id, wbuf, max_chars);
    utf8 = g_utf16_to_utf8((const gunichar2 *)wbuf, -1, NULL, NULL, NULL);
    g_free(wbuf);
    return utf8;
}

void gtv_win_menu_add_utf8(HMENU hmenu, const gchar *utf8, UINT flags, UINT_PTR id) {
    if (flags & MF_SEPARATOR) {
        InsertMenuW(hmenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        return;
    }

    gunichar2 *w = g_utf8_to_utf16(utf8 ? utf8 : "", -1, NULL, NULL, NULL);
    InsertMenuW(hmenu, -1, MF_BYPOSITION | MF_STRING | flags, id, (LPCWSTR)w);
    g_free(w);
}
