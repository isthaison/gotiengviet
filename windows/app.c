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
    gtv_app_save_enabled();
}

static void gtv_app_save_enabled(void) {
    gchar *path = gtv_app_config_path();
    GKeyFile *kf = g_key_file_new();
    g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
    g_key_file_set_boolean(kf, "input", "enabled", g_app.enabled);
    gchar *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    g_key_file_save_to_file(kf, path, NULL);
    g_key_file_unref(kf);
    g_free(path);
}

gboolean gtv_app_load_enabled(gboolean def) {
    gchar *path = gtv_app_config_path();
    GKeyFile *kf = g_key_file_new();
    gboolean enabled = def;
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)
        && g_key_file_has_key(kf, "input", "enabled", NULL))
        enabled = g_key_file_get_boolean(kf, "input", "enabled", NULL);
    g_key_file_unref(kf);
    g_free(path);
    return enabled;
}

void gtv_app_set_mode(gboolean enabled) {
    g_app.enabled = enabled;
    gtv_tray_update_icon(g_app.enabled);
    gtv_app_save_enabled();
}

void gtv_app_toggle_mode(void) {
    gtv_app_set_mode(!g_app.enabled);
}

void gtv_app_set_input_method(GtvMode mode) {
    g_app.config.mode = mode;
    gtv_app_save_config();
}
