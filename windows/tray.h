#ifndef TRAY_H
#define TRAY_H

#include <windows.h>
#include <glib.h>

#define WM_TRAY_CALLBACK (WM_USER + 1)
#define WM_GTV_AI_RESULT (WM_APP + 100)
#ifndef NIN_BALLOONUSERCLICK
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif

gboolean gtv_tray_init(HWND hwnd);
void gtv_tray_cleanup(void);
void gtv_tray_update_icon(gboolean enabled);
void gtv_tray_show_menu(HWND hwnd);
void gtv_tray_balloon(const gchar *title, const gchar *msg);
gboolean gtv_tray_startup_enabled(void);
void gtv_tray_set_startup(gboolean enable);
/* Same balloon without the 10s anti-spam throttle, for rare update
 * events where silence looks like a broken feature. */
void gtv_tray_balloon_force(const gchar *title, const gchar *msg);
void gtv_config_strings_lock(void);
void gtv_config_strings_unlock(void);

#endif /* TRAY_H */
