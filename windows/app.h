#ifndef APP_H
#define APP_H

#include <windows.h>
#include <glib.h>
#include "../engine/engine.h"

typedef struct {
    gboolean enabled;       /* TRUE = Vietnamese [V], FALSE = English [E] */
    GtvConfig config;
    HWND hwnd_main;
} GtvWindowsApp;

extern GtvWindowsApp g_app;

/* Message-only window class shared with gtv_tsf.dll (it notifies the
 * tray about committed words for AI checks). */
#define GTV_TRAY_WINDOW_CLASS "GoTiengViet_Message_Window"
/* WM_COPYDATA dwData for a committed word (NUL-terminated UTF-8). */
#define GTV_AI_COPYDATA_ID 0x47545641

void gtv_app_toggle_mode(void);
void gtv_app_set_mode(gboolean enabled);
void gtv_app_set_input_method(GtvMode mode);
void gtv_app_save_config(void);
void gtv_app_save_enabled(void);
gboolean gtv_app_load_enabled(gboolean def);
gchar *gtv_app_config_path(void);

#endif /* APP_H */
