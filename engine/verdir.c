#include "verdir.h"

#include <string.h>
#include <glib/gstdio.h>

/* Basename of a path (handles both separators). Points inside path. */
static const gchar *base_name(const gchar *path) {
    const gchar *b = path;
    for (const gchar *p = path; *p; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (c == '/' || c == '\\') b = g_utf8_next_char(p);
    }
    return b;
}

gchar *gtv_app_dir_for_module(const gchar *modpath) {
    if (!modpath || !*modpath) return g_strdup(".");
    gchar *dir = g_path_get_dirname(modpath);
    gchar *parent = g_path_get_dirname(dir);
    const gchar *grandbase = base_name(parent);
    gboolean versioned = g_ascii_strcasecmp(grandbase, "ver") == 0;
    gchar *appdir;
    if (versioned) {
        appdir = g_path_get_dirname(parent);
    } else {
        appdir = dir;
        dir = NULL;
    }
    g_free(dir);
    g_free(parent);
    return appdir;
}

gchar *gtv_ver_current(const gchar *appdir) {
    if (!appdir || !*appdir) return NULL;
    gchar *path = g_build_filename(appdir, "current.txt", NULL);
    gchar *contents = NULL;
    gsize len = 0;
    if (!g_file_get_contents(path, &contents, &len, NULL)) {
        g_free(path);
        return NULL;
    }
    g_free(path);
    g_strstrip(contents);
    if (!*contents) {
        g_free(contents);
        return NULL;
    }
    /* First line only; tolerate trailing newlines/whitespace. */
    gchar *nl = strchr(contents, '\n');
    if (nl) *nl = '\0';
    g_strstrip(contents);
    if (!*contents) {
        g_free(contents);
        return NULL;
    }
    return contents;
}

gchar *gtv_ver_dir(const gchar *appdir, const gchar *ver) {
    if (!appdir || !*appdir) return g_strdup(".");
    if (!ver || !*ver) return g_strdup(appdir);
    return g_build_filename(appdir, "ver", ver, NULL);
}

gchar *gtv_forward_target(const gchar *appdir, const gchar *own_ver) {
    gchar *current = gtv_ver_current(appdir);
    if (!current) return NULL;
    gchar *target = NULL;
    if (gtv_ver_cmp(current, own_ver ? own_ver : "") > 0) {
        gchar *dir = gtv_ver_dir(appdir, current);
        gchar *exe = g_build_filename(dir, "gotiengviet.exe", NULL);
        if (g_file_test(exe, G_FILE_TEST_IS_EXECUTABLE))
            target = exe;
        else
            g_free(exe);
        g_free(dir);
    }
    g_free(current);
    return target;
}

gint gtv_ver_cmp(const gchar *a, const gchar *b) {
    for (int i = 0; i < 4; i++) {
        long va = 0, vb = 0;
        if (a && *a) {
            va = strtol(a, NULL, 10);
            const gchar *dot = strchr(a, '.');
            a = dot ? dot + 1 : "";
        } else a = "";
        if (b && *b) {
            vb = strtol(b, NULL, 10);
            const gchar *dot = strchr(b, '.');
            b = dot ? dot + 1 : "";
        } else b = "";
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}

static gboolean remove_tree(const gchar *path) {
    GError *err = NULL;
    GDir *dir = g_dir_open(path, 0, &err);
    if (!dir) {
        g_clear_error(&err);
        /* Not a dir (or unreadable): try plain file delete. */
        return g_remove(path) == 0;
    }
    gboolean ok = TRUE;
    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        gchar *child = g_build_filename(path, name, NULL);
        if (!remove_tree(child)) ok = FALSE;
        g_free(child);
    }
    g_dir_close(dir);
    if (ok && g_remove(path) != 0) ok = FALSE;
    return ok;
}

guint gtv_ver_cleanup(const gchar *appdir) {
    if (!appdir || !*appdir) return 0;
    gchar *verbase = g_build_filename(appdir, "ver", NULL);
    GError *err = NULL;
    GDir *dir = g_dir_open(verbase, 0, &err);
    if (!dir) {
        g_clear_error(&err);
        g_free(verbase);
        return 0;
    }
    gchar *current = gtv_ver_current(appdir);
    /* Newest non-current payload is kept as instant rollback. */
    gchar *newest_other = NULL;
    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    const gchar *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        gchar *full = g_build_filename(verbase, name, NULL);
        if (!g_file_test(full, G_FILE_TEST_IS_DIR)) {
            g_free(full);
            continue;
        }
        g_free(full);
        g_ptr_array_add(names, g_strdup(name));
        if ((!current || strcmp(name, current) != 0) &&
            (!newest_other || gtv_ver_cmp(name, newest_other) > 0)) {
            g_free(newest_other);
            newest_other = g_strdup(name);
        }
    }
    g_dir_close(dir);
    guint removed = 0;
    for (guint i = 0; i < names->len; i++) {
        const gchar *v = g_ptr_array_index(names, i);
        if (current && !strcmp(v, current)) continue;
        if (newest_other && !strcmp(v, newest_other)) continue;
        gchar *full = g_build_filename(verbase, v, NULL);
        if (remove_tree(full)) removed++;
        g_free(full);
    }
    g_ptr_array_unref(names);
    g_free(newest_other);
    g_free(current);
    g_free(verbase);
    return removed;
}

void gtv_legacy_cleanup(const gchar *appdir) {
    if (!appdir || !*appdir) return;
    gchar *verbase = g_build_filename(appdir, "ver", NULL);
    gchar *curpath = g_build_filename(appdir, "current.txt", NULL);
    gboolean migrated = g_file_test(verbase, G_FILE_TEST_IS_DIR)
                     && g_file_test(curpath, G_FILE_TEST_IS_REGULAR);
    g_free(verbase);
    g_free(curpath);
    if (!migrated) return;
    /* Flat-layout payload that lived at {app} root before side-by-side,
     * plus the rename-aside spare from friendly migrations
     * (gtv_tsf.prev.dll never blocks an install; deleted when free). */
    static const gchar *legacy_files[] = {
        "gotiengviet.exe",
        "gtv_tsf.prev.dll",
        "gspawn-win64-helper.exe", "gspawn-win64-helper-console.exe",
        "libffi-8.dll", "libgcc_s_seh-1.dll", "libgio-2.0-0.dll",
        "libglib-2.0-0.dll", "libgmodule-2.0-0.dll", "libgobject-2.0-0.dll",
        "libiconv-2.dll", "libintl-8.dll", "libpcre2-8-0.dll",
        "libstdc++-6.dll", "libwinpthread-1.dll", "zlib1.dll",
        NULL,
    };
    for (guint i = 0; legacy_files[i]; i++) {
        gchar *p = g_build_filename(appdir, legacy_files[i], NULL);
        /* Locked files fail silently; retried on the next launch. */
        g_remove(p);
        g_free(p);
    }
    gchar *data = g_build_filename(appdir, "data", NULL);
    if (g_file_test(data, G_FILE_TEST_IS_DIR)) remove_tree(data);
    g_free(data);
}
