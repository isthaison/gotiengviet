#include "engine.h"
#include <glib/gstdio.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif

/* Shared data-file resolution: $GTV_DATA_DIR (tests/dev override) →
 * user config dir → /usr/share/gotiengviet (installed) → ./data. */
gchar *gtv_data_path(const gchar *name){
    const gchar *env = g_getenv("GTV_DATA_DIR");
    if(env && *env) return g_build_filename(env, name, NULL);
    gchar *user = g_build_filename(g_get_user_config_dir(), "gotiengviet", name, NULL);
    if(g_file_test(user, G_FILE_TEST_EXISTS)) return user;
    g_free(user);
#ifdef G_OS_WIN32
    /* Check next to the running module (gtv_tsf.dll or gotiengviet.exe) */
    {
        HMODULE hDll = NULL;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCSTR)gtv_data_path, &hDll) || !hDll) {
            hDll = GetModuleHandleA("gtv_tsf.dll");
        }
        if (hDll) {
            char dllpath[MAX_PATH];
            if (GetModuleFileNameA(hDll, dllpath, sizeof(dllpath))) {
                gchar *dlldir = g_path_get_dirname(dllpath);
                gchar *bundled = g_build_filename(dlldir, "data", name, NULL);
                if (g_file_test(bundled, G_FILE_TEST_EXISTS)) { g_free(dlldir); return bundled; }
                g_free(bundled);
                bundled = g_build_filename(dlldir, "..", "data", name, NULL);
                if (g_file_test(bundled, G_FILE_TEST_EXISTS)) { g_free(dlldir); return bundled; }
                g_free(bundled);
                bundled = g_build_filename(dlldir, "..", "..", "data", name, NULL);
                if (g_file_test(bundled, G_FILE_TEST_EXISTS)) { g_free(dlldir); return bundled; }
                g_free(bundled);
                g_free(dlldir);
            }
        }
    }
    /* Portable install: data/ next to the .exe (when running gotiengviet.exe) */
    {
        char exepath[MAX_PATH];
        if(GetModuleFileNameA(NULL, exepath, sizeof(exepath))){
            gchar *exedir = g_path_get_dirname(exepath);
            gchar *bundled = g_build_filename(exedir, "data", name, NULL);
            if(g_file_test(bundled, G_FILE_TEST_EXISTS)) { g_free(exedir); return bundled; }
            g_free(bundled);
            bundled = g_build_filename(exedir, "..", "data", name, NULL);
            if(g_file_test(bundled, G_FILE_TEST_EXISTS)) { g_free(exedir); return bundled; }
            g_free(bundled);
            bundled = g_build_filename(exedir, "..", "..", "data", name, NULL);
            if(g_file_test(bundled, G_FILE_TEST_EXISTS)) { g_free(exedir); return bundled; }
            g_free(bundled);
            g_free(exedir);
        }
    }
    /* Registry lookup for registered InprocServer32 directory */
    {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Classes\\CLSID\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\InprocServer32", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
            RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\CLSID\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\InprocServer32", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char regpath[MAX_PATH] = {0};
            DWORD size = sizeof(regpath);
            if (RegQueryValueExA(hKey, NULL, NULL, NULL, (LPBYTE)regpath, &size) == ERROR_SUCCESS && *regpath) {
                gchar *regdir = g_path_get_dirname(regpath);
                gchar *bundled = g_build_filename(regdir, "data", name, NULL);
                if (g_file_test(bundled, G_FILE_TEST_EXISTS)) { RegCloseKey(hKey); g_free(regdir); return bundled; }
                g_free(bundled);
                bundled = g_build_filename(regdir, "..", "data", name, NULL);
                if (g_file_test(bundled, G_FILE_TEST_EXISTS)) { RegCloseKey(hKey); g_free(regdir); return bundled; }
                g_free(bundled);
                bundled = g_build_filename(regdir, "..", "..", "data", name, NULL);
                if (g_file_test(bundled, G_FILE_TEST_EXISTS)) { RegCloseKey(hKey); g_free(regdir); return bundled; }
                g_free(bundled);
                g_free(regdir);
            }
            RegCloseKey(hKey);
        }
    }
#endif
    gchar *system = g_build_filename("/usr", "share", "gotiengviet", name, NULL);
    if(g_file_test(system, G_FILE_TEST_EXISTS)) return system;
    g_free(system);
    return g_build_filename("data", name, NULL);
}

static GKeyFile *prompts_kf = NULL;
static void prompts_load(void){
    if(prompts_kf) return;
    prompts_kf = g_key_file_new();
    gchar *path = gtv_data_path("prompts.conf");
    g_key_file_load_from_file(prompts_kf, path, G_KEY_FILE_NONE, NULL);
    g_free(path);
}
void gtv_prompts_reload(void){
    if(prompts_kf) g_key_file_unref(prompts_kf);
    prompts_kf = NULL;
}
gchar *gtv_prompt_get(const gchar *group, const gchar *key, const gchar *fallback){
    prompts_load();
    gchar *v = g_key_file_get_string(prompts_kf, group, key, NULL);
    if(!v && fallback) v = g_strdup(fallback);
    return v;
}
/* Substitute the first n %s occurrences in order; every other byte
 * (including stray % or %%) is copied literally, so user templates with
 * extra % signs cannot crash or misbehave. */
gchar *gtv_format_template(const gchar *templ, const gchar * const *args, guint n){
    if(!templ) return g_strdup("");
    GString *out = g_string_new("");
    guint used = 0;
    for(const gchar *p = templ; *p; ){
        if(p[0] == '%' && p[1] == 's' && used < n){
            const gchar *a = (args && args[used]) ? args[used] : "";
            g_string_append(out, a);
            used++;
            p += 2;
        }else{
            g_string_append_unichar(out, g_utf8_get_char(p));
            p = g_utf8_next_char(p);
        }
    }
    return g_string_free(out, FALSE);
}

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
static void overlay_file(GKeyFile *file, GtvConfig *config, gboolean input, gboolean provider) {
    if (input) {
        gchar *method = g_key_file_get_string(file, "input", "method", NULL);
        config->mode = method && !g_ascii_strcasecmp(method, "vni") ? GTV_VNI : GTV_TELEX;
        g_free(method);
        read_bool(file, "input", "modern", &config->modern);
        read_bool(file, "input", "spellcheck", &config->spellcheck);
    }
    read_bool(file, "ai", "enable", &config->ai_enabled);
    read_string(file, "ai", "model", &config->model);
    read_string(file, "ai", "url", &config->url);
    read_string(file, "ai", "port", &config->port);
    if (provider) {
        gchar *prov = g_key_file_get_string(file, "ai", "provider", NULL);
        if (g_strcmp0(prov, "ollama") == 0) config->ai_enabled = TRUE;
        if (g_strcmp0(prov, "rule") == 0) config->ai_enabled = FALSE;
        g_free(prov);
    }
}
static void overlay_path(GtvConfig *config, const gchar *path, gboolean input, gboolean provider) {
    GKeyFile *file = g_key_file_new();
    if (g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL))
        overlay_file(file, config, input, provider);
    g_key_file_unref(file);
}
void gtv_config_load(GtvConfig *config, const gchar *directory) {
    *config = (GtvConfig){.mode = GTV_TELEX, .modern = TRUE, .spellcheck = TRUE,
        .model = g_strdup("qwen2:0.5b"), .url = g_strdup("http://localhost:55602"), .port = g_strdup("55602")};
    /* Shipped defaults first (data/config, data/ai.conf), then user files.
     * ai.conf values take precedence over the [ai] section of config. */
    gchar *path = gtv_data_path("config");
    overlay_path(config, path, TRUE, FALSE);
    g_free(path);
    path = g_build_filename(directory, "config", NULL);
    overlay_path(config, path, TRUE, FALSE);
    g_free(path);
    path = gtv_data_path("ai.conf");
    overlay_path(config, path, FALSE, TRUE);
    g_free(path);
    path = g_build_filename(directory, "ai.conf", NULL);
    overlay_path(config, path, FALSE, TRUE);
    g_free(path);
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
