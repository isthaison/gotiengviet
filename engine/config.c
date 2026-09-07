#include "engine.h"
#include <glib/gstdio.h>

static void read_string(GKeyFile *file, const gchar *group, const gchar *key, gchar **value) {
    gchar *next = g_key_file_get_string(file, group, key, NULL);
    if (next && *g_strstrip(next)) { g_free(*value); *value = next; }
    else g_free(next);
}
static void read_bool(GKeyFile *file, const gchar *group, const gchar *key, gboolean *value) {
    gchar *next = g_key_file_get_string(file,group,key,NULL);
    if (!next) return;
    g_strstrip(next);
    if (!g_ascii_strcasecmp(next,"true") || !strcmp(next,"1")) *value=TRUE;
    if (!g_ascii_strcasecmp(next,"false") || !strcmp(next,"0")) *value=FALSE;
    g_free(next);
}
void gtv_config_load(GtvConfig *config, const gchar *directory) {
    *config = (GtvConfig){.mode = GTV_TELEX, .modern = TRUE, .spellcheck = TRUE,
        .model = g_strdup("qwen2:0.5b"), .url = g_strdup("http://localhost:55602"), .port = g_strdup("55602")};
    GKeyFile *file = g_key_file_new();
    gchar *path = g_build_filename(directory, "config", NULL);
    if (g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL)) {
        gchar *method = g_key_file_get_string(file, "input", "method", NULL);
        config->mode = method && !g_ascii_strcasecmp(method, "vni") ? GTV_VNI : GTV_TELEX;
        g_free(method);
        read_bool(file, "input", "modern", &config->modern);
        read_bool(file, "input", "spellcheck", &config->spellcheck);
        read_bool(file, "ai", "enable", &config->ai_enabled);
        read_string(file, "ai", "model", &config->model);
        read_string(file, "ai", "url", &config->url);
        read_string(file, "ai", "port", &config->port);
    }
    g_free(path);
    g_key_file_unref(file);
    file = g_key_file_new();
    path = g_build_filename(directory, "ai.conf", NULL);
    if (g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL)) {
        read_string(file, "ai", "model", &config->model);
        read_string(file, "ai", "url", &config->url);
        read_string(file, "ai", "port", &config->port);
        read_bool(file, "ai", "enable", &config->ai_enabled);
        gchar *provider = g_key_file_get_string(file, "ai", "provider", NULL);
        if (g_strcmp0(provider, "ollama") == 0) config->ai_enabled = TRUE;
        if (g_strcmp0(provider, "rule") == 0) config->ai_enabled = FALSE;
        g_free(provider);
    }
    g_free(path);
    g_key_file_unref(file);
    /* Older setup versions stored the display label instead of the model ID. */
    gchar *label=strstr(config->model," (~");
    if(label && g_str_has_suffix(config->model,")"))*label='\0';
}

gboolean gtv_config_save(const GtvConfig *config, const gchar *directory, GError **error) {
    if (g_mkdir_with_parents(directory, 0755) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Cannot create %s: %s", directory, g_strerror(errno));
        return FALSE;
    }
    GKeyFile *file = g_key_file_new();
    gchar *path = g_build_filename(directory, "config", NULL);
    g_key_file_load_from_file(file, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
    g_key_file_set_string(file, "input", "method", config->mode == GTV_VNI ? "vni" : "telex");
    g_key_file_set_boolean(file, "input", "modern", config->modern);
    g_key_file_set_boolean(file, "input", "spellcheck", config->spellcheck);
    g_key_file_set_string(file, "input", "charset", "unicode");
    g_key_file_set_boolean(file, "ai", "enable", config->ai_enabled);
    g_key_file_set_string(file, "ai", "model", config->model);
    g_key_file_set_string(file, "ai", "url", config->url);
    g_key_file_set_string(file, "ai", "port", config->port);
    gboolean ok = g_key_file_save_to_file(file, path, error);
    g_free(path);
    g_key_file_unref(file);
    if (!ok) return FALSE;
    file = g_key_file_new();
    g_key_file_set_string(file, "ai", "provider", config->ai_enabled ? "ollama" : "rule");
    g_key_file_set_string(file, "ai", "model", config->model);
    g_key_file_set_string(file, "ai", "url", config->url);
    g_key_file_set_string(file, "ai", "port", config->port);
    path = g_build_filename(directory, "ai.conf", NULL);
    ok = g_key_file_save_to_file(file, path, error);
    g_free(path);
    g_key_file_unref(file);
    return ok;
}
void gtv_config_clear(GtvConfig *config) {
    g_free(config->model); g_free(config->url); g_free(config->port);
    *config = (GtvConfig){0};
}
