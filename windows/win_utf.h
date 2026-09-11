#ifndef GTV_WIN_UTF_H
#define GTV_WIN_UTF_H

#include <windows.h>
#include <glib.h>

void gtv_win_copy_utf8(WCHAR *dst, guint dst_chars, const gchar *utf8);
void gtv_win_set_window_text(HWND hwnd, const gchar *utf8);
void gtv_win_set_dlg_item_text(HWND hwnd, int id, const gchar *utf8);
gchar *gtv_win_get_dlg_item_text(HWND hwnd, int id, int max_chars);
void gtv_win_menu_add_utf8(HMENU hmenu, const gchar *utf8, UINT flags, UINT_PTR id);

#endif /* GTV_WIN_UTF_H */
