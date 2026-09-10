#ifndef APP_H
#define APP_H

#include <windows.h>
#include <glib.h>
#include "../engine/engine.h"

typedef struct {
    gboolean enabled;       /* TRUE = Vietnamese [V], FALSE = English [E] */
    GtvConfig config;
    HWND hwnd_main;
    DWORD last_input_tick;
} GtvWindowsApp;

extern GtvWindowsApp g_app;

void gtv_app_toggle_mode(void);
void gtv_app_set_mode(gboolean enabled);
void gtv_app_set_input_method(GtvMode mode);
void gtv_app_save_config(void);
void gtv_app_save_enabled(void);
gboolean gtv_app_load_enabled(gboolean def);
gchar *gtv_app_config_path(void);

#endif /* APP_H */
