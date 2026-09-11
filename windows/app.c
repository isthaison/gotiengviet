#include "app.h"
#include "tray.h"
#include <glib/gstdio.h>

gchar *gtv_app_config_path(void) {
    return g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
}

void gtv_app_save_config(void) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gtv_config_save(&g_app.config, dir, NULL);
    g_free(dir);
}

void gtv_app_set_input_method(GtvMode mode) {
    g_app.config.mode = mode;
    gtv_tray_update_icon();
    gtv_app_save_config();
}
