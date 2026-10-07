#include "engine.h"
#include <glib/gstdio.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif

#ifdef G_OS_WIN32
/* This DLL's own CLSID key (must match windows/tsf/tsf_defs.h). */
#define GTV_TSF_CLSID_KEY L"Software\\Classes\\CLSID\\{E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}\\InprocServer32"

/* Module/registry paths may contain non-ASCII (usernames!): stay in
 * UTF-16, convert once to UTF-8 for glib. */
static gchar *module_dir(HMODULE mod) {
    WCHAR wpath[MAX_PATH];
    if (!GetModuleFileNameW(mod, wpath, MAX_PATH)) return NULL;
    gchar *u8 = g_utf16_to_utf8(wpath, -1, NULL, NULL, NULL);
    if (!u8) return NULL;
    gchar *dir = g_path_get_dirname(u8);
    g_free(u8);
    return dir;
}
/* data/<name> next to dir, else one/two levels up (dev layouts). */
static gchar *bundled_below(const gchar *dir, const gchar *name) {
    gchar *bundled = g_build_filename(dir, "data", name, NULL);
    if (g_file_test(bundled, G_FILE_TEST_EXISTS)) return bundled;
    g_free(bundled);
    bundled = g_build_filename(dir, "..", "data", name, NULL);
    if (g_file_test(bundled, G_FILE_TEST_EXISTS)) return bundled;
    g_free(bundled);
    bundled = g_build_filename(dir, "..", "..", "data", name, NULL);
    if (g_file_test(bundled, G_FILE_TEST_EXISTS)) return bundled;
    g_free(bundled);
    return NULL;
}
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
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCWSTR)gtv_data_path, &hDll) || !hDll) {
            /* Versioned engine first, then the stable stub (same dir as
             * current.txt), then the legacy single-dir layout. */
            hDll = GetModuleHandleW(L"gtv_engine.dll");
            if (!hDll) hDll = GetModuleHandleW(L"gtv_tsf.dll");
        }
        if (hDll) {
            gchar *dlldir = module_dir(hDll);
            if (dlldir) {
                gchar *bundled = bundled_below(dlldir, name);
                g_free(dlldir);
                if (bundled) return bundled;
            }
        }
    }
    /* Portable install: data/ next to the .exe (when running gotiengviet.exe) */
    {
        gchar *exedir = module_dir(NULL);
        if (exedir) {
            gchar *bundled = bundled_below(exedir, name);
            g_free(exedir);
            if (bundled) return bundled;
        }
    }
    /* Registry lookup for registered InprocServer32 directory */
    {
        HKEY hKey = NULL;
        LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, GTV_TSF_CLSID_KEY, 0, KEY_READ, &hKey);
        if (rc != ERROR_SUCCESS)
            rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, GTV_TSF_CLSID_KEY, 0, KEY_READ, &hKey);
        if (rc == ERROR_SUCCESS) {
            WCHAR wreg[MAX_PATH] = {0};
            DWORD size = sizeof(wreg);
            if (RegQueryValueExW(hKey, NULL, NULL, NULL, (LPBYTE)wreg, &size) == ERROR_SUCCESS && *wreg) {
                gchar *ureg = g_utf16_to_utf8(wreg, -1, NULL, NULL, NULL);
                if (ureg) {
                    gchar *regdir = g_path_get_dirname(ureg);
                    g_free(ureg);
                    gchar *bundled = bundled_below(regdir, name);
                    g_free(regdir);
                    RegCloseKey(hKey);
                    if (bundled) return bundled;
                }
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

static void read_bool(GKeyFile *file, const gchar *group, const gchar *key, gboolean *value) {
    gchar *next = g_key_file_get_string(file,group,key,NULL);
    if (!next) return;
    g_strstrip(next);
    if (!g_ascii_strcasecmp(next,"true") || !strcmp(next,"1")) *value=TRUE;
    if (!g_ascii_strcasecmp(next,"false") || !strcmp(next,"0")) *value=FALSE;
    g_free(next);
}
/* Suggestions used to live behind an [ai] section (Ollama provider).
 * The dictionary backend reads [suggest] enable; legacy [ai] enable /
 * provider values still migrate (provider=ollama implies on) so existing
 * user configs keep working. */
static void overlay_suggest(GKeyFile *file, GtvConfig *config) {
    if (g_key_file_has_key(file, "suggest", "enable", NULL)) {
        read_bool(file, "suggest", "enable", &config->suggest_enabled);
        return;
    }
    if (g_key_file_has_key(file, "ai", "enable", NULL)) {
        read_bool(file, "ai", "enable", &config->suggest_enabled);
        return;
    }
    gchar *prov = g_key_file_get_string(file, "ai", "provider", NULL);
    if (prov) {
        g_strstrip(prov);
        if (!g_strcmp0(prov, "ollama")) config->suggest_enabled = TRUE;
        if (!g_strcmp0(prov, "rule")) config->suggest_enabled = FALSE;
        g_free(prov);
    }
}
static void overlay_file(GKeyFile *file, GtvConfig *config, gboolean input) {
    if (input) {
        gchar *method = g_key_file_get_string(file, "input", "method", NULL);
        config->mode = method && !g_ascii_strcasecmp(method, "vni") ? GTV_VNI : GTV_TELEX;
        g_free(method);
        read_bool(file, "input", "modern", &config->modern);
        read_bool(file, "input", "spellcheck", &config->spellcheck);
    }
    overlay_suggest(file, config);
}
static void overlay_path(GtvConfig *config, const gchar *path, gboolean input) {
    GKeyFile *file = g_key_file_new();
    if (g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL))
        overlay_file(file, config, input);
    g_key_file_unref(file);
}
void gtv_config_load(GtvConfig *config, const gchar *directory) {
    *config = (GtvConfig){.mode = GTV_TELEX, .modern = TRUE, .spellcheck = TRUE,
        .suggest_enabled = TRUE};
    /* Shipped defaults first (data/config), then user files. */
    gchar *path = gtv_data_path("config");
    overlay_path(config, path, TRUE);
    g_free(path);
    path = g_build_filename(directory, "config", NULL);
    overlay_path(config, path, TRUE);
    g_free(path);
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
    g_key_file_set_boolean(file, "suggest", "enable", config->suggest_enabled);
    gboolean ok = g_key_file_save_to_file(file, path, error);
    g_free(path);
    g_key_file_unref(file);
    return ok;
}
void gtv_config_clear(GtvConfig *config) {
    *config = (GtvConfig){0};
}
