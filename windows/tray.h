#ifndef TRAY_H
#define TRAY_H

#include <windows.h>
#include <glib.h>

#define WM_TRAY_CALLBACK (WM_USER + 1)
#define WM_GTV_AI_RESULT (WM_APP + 100)

gboolean gtv_tray_init(HWND hwnd);
void gtv_tray_cleanup(void);
void gtv_tray_update_icon(gboolean enabled);
void gtv_tray_show_menu(HWND hwnd);
void gtv_tray_balloon(const gchar *title, const gchar *msg);
void gtv_config_strings_lock(void);
void gtv_config_strings_unlock(void);
/* Async Ollama typo check for a just-committed word; shows a balloon with
 * the top correction, if any. Safe to call from the keyboard hook. */
void gtv_tray_check_spelling_async(const gchar *word);

#endif /* TRAY_H */
