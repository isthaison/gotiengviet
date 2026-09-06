#ifndef TRAY_H
#define TRAY_H

#include <windows.h>
#include <glib.h>

#define WM_TRAY_CALLBACK (WM_USER + 1)

gboolean gtv_tray_init(HWND hwnd);
void gtv_tray_cleanup(void);
void gtv_tray_update_icon(gboolean enabled);
void gtv_tray_show_menu(HWND hwnd);

#endif /* TRAY_H */
