/* Self-update support: compare versions against GitHub releases.
 * Only glib is used so Windows and tests share this code. Network goes
 * through curl on stdin/argv (never a shell), like engine/ai.c. */
#include "internal.h"
#include <gio/gio.h>
#include <glib/gstdio.h>

#ifndef GTV_GITHUB_REPO
#define GTV_GITHUB_REPO "isthaison/gotiengviet"
#endif
#define GTV_UPDATE_INTERVAL_SECS (24 * 3600)

static const gchar *strip_tag(const gchar *v) {
    if (!v) return "";
    while (g_ascii_isspace(*v)) v++;
    if (*v == 'v' || *v == 'V') v++;
    return v;
}

/* Numeric prefix comparison; a version without suffix (release) beats the
 * same numbers with a suffix (prerelease); two suffixes compare lexically. */
gint gtv_version_compare(const gchar *a, const gchar *b) {
    const gchar *pa = strip_tag(a), *pb = strip_tag(b);
    for (;;) {
        if (!g_ascii_isdigit(*pa) || !g_ascii_isdigit(*pb)) break;
        gchar *enda = NULL, *endb = NULL;
        gint64 na = g_ascii_strtoll(pa, &enda, 10);
        gint64 nb = g_ascii_strtoll(pb, &endb, 10);
        if (na != nb) return na < nb ? -1 : 1;
        pa = enda; pb = endb;
        gboolean da = *pa == '.', db = *pb == '.';
        if (da && db) { pa++; pb++; continue; }
        if (da) {
            /* a has more parts: remaining must be all zero to stay equal. */
            pa++;
            while (g_ascii_isdigit(*pa)) {
                gchar *end = NULL;
                if (g_ascii_strtoll(pa, &end, 10) != 0) return 1;
                pa = end;
                if (*pa != '.') break;
                pa++;
            }
            break;
        }
        if (db) {
            pb++;
            while (g_ascii_isdigit(*pb)) {
                gchar *end = NULL;
                if (g_ascii_strtoll(pb, &end, 10) != 0) return -1;
                pb = end;
                if (*pb != '.') break;
                pb++;
            }
            break;
        }
        break;
    }
    gboolean sa = *pa && *pa != ' ' && *pa != '\t' && *pa != '\r' && *pa != '\n';
    gboolean sb = *pb && *pb != ' ' && *pb != '\t' && *pb != '\r' && *pb != '\n';
    if (!sa && !sb) return 0;
    if (!sa) return 1;   /* release beats prerelease with same numbers */
    if (!sb) return -1;
    return g_strcmp0(pa, pb);
}

gchar *gtv_update_asset_name(const gchar *version) {
    return g_strdup_printf("gotiengviet-%s-x64-setup.exe", strip_tag(version));
}

/* --- Minimal JSON string scanner for the GitHub release payload. --- */
static gboolean append_escaped(GString *out, const gchar **p) {
    gchar esc = *(*p)++;
    switch (esc) {
    case '"': case '\\': case '/': g_string_append_c(out, esc); return TRUE;
    case 'b': g_string_append_c(out, '\b'); return TRUE;
    case 'f': g_string_append_c(out, '\f'); return TRUE;
    case 'n': g_string_append_c(out, '\n'); return TRUE;
    case 'r': g_string_append_c(out, '\r'); return TRUE;
    case 't': g_string_append_c(out, '\t'); return TRUE;
    case 'u': {
        gunichar code = 0;
        for (int i = 0; i < 4; i++) {
            int d = g_ascii_xdigit_value(*(*p)++);
            if (d < 0) return FALSE;
            code = (code << 4) | (gunichar)d;
        }
        if (code >= 0xd800 && code <= 0xdbff) {
            gunichar low = 0;
            if (*(*p)++ != '\\' || *(*p)++ != 'u') return FALSE;
            for (int i = 0; i < 4; i++) {
                int d = g_ascii_xdigit_value(*(*p)++);
                if (d < 0) return FALSE;
                low = (low << 4) | (gunichar)d;
            }
            if (low < 0xdc00 || low > 0xdfff) return FALSE;
            code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
        }
        if (!code || !g_unichar_validate(code)) return FALSE;
        g_string_append_unichar(out, code);
        return TRUE;
    }
    default: return FALSE;
    }
}

/* If *p is at a JSON string, consume it and return the unescaped value. */
static gchar *scan_string(const gchar **p) {
    if (**p != '"') return NULL;
    (*p)++;
    GString *out = g_string_new("");
    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p)++;
            if (!**p || !append_escaped(out, p)) { g_string_free(out, TRUE); return NULL; }
        } else {
            if ((guchar)**p < 0x20) { g_string_free(out, TRUE); return NULL; }
            g_string_append_c(out, **p);
            (*p)++;
        }
    }
    if (**p != '"') { g_string_free(out, TRUE); return NULL; }
    (*p)++;
    if (!g_utf8_validate(out->str, out->len, NULL)) { g_string_free(out, TRUE); return NULL; }
    return g_string_free(out, FALSE);
}

static void skip_space(const gchar **p) {
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') (*p)++;
}

/* Skip one JSON value; returns FALSE on malformed input. */
static gboolean skip_value(const gchar **p, guint depth) {
    if (depth > 64) return FALSE;
    skip_space(p);
    if (**p == '"') {
        gchar *s = scan_string(p);
        if (!s) return FALSE;
        g_free(s);
        return TRUE;
    }
    if (**p == '{' || **p == '[') {
        gchar open = **p, close = open == '{' ? '}' : ']';
        (*p)++;
        skip_space(p);
        if (**p == close) { (*p)++; return TRUE; }
        while (**p) {
            if (open == '{') {
                gchar *k = scan_string(p);
                if (!k) return FALSE;
                g_free(k);
                skip_space(p);
                if (**p != ':') return FALSE;
                (*p)++;
            }
            if (!skip_value(p, depth + 1)) return FALSE;
            skip_space(p);
            if (**p == close) { (*p)++; return TRUE; }
            if (**p != ',') return FALSE;
            (*p)++;
        }
        return FALSE;
    }
    if (**p == '\0') return FALSE;
    for (guint i = 0; i < 3; i++) {
        const gchar *lit[] = {"true", "false", "null"};
        if (g_str_has_prefix(*p, lit[i])) { *p += strlen(lit[i]); return TRUE; }
    }
    if (**p == '-') (*p)++;
    if (!g_ascii_isdigit(**p)) return FALSE;
    while (g_ascii_isdigit(**p)) (*p)++;
    if (**p == '.') {
        (*p)++;
        if (!g_ascii_isdigit(**p)) return FALSE;
        while (g_ascii_isdigit(**p)) (*p)++;
    }
    if (**p == 'e' || **p == 'E') {
        (*p)++;
        if (**p == '+' || **p == '-') (*p)++;
        if (!g_ascii_isdigit(**p)) return FALSE;
        while (g_ascii_isdigit(**p)) (*p)++;
    }
    return TRUE;
}

/* Find the string value of "key" in the object starting at *p (p at '{').
 * On success sets *value and *end (just past the value), else NULLs. */
static void object_string(const gchar *obj, const gchar *key, gchar **value, const gchar **end) {
    *value = NULL;
    if (end) *end = NULL;
    const gchar *p = obj;
    if (*p != '{') return;
    p++;
    for (;;) {
        skip_space(&p);
        if (*p == '}') return;
        gchar *k = scan_string(&p);
        if (!k) return;
        gboolean want = !strcmp(k, key);
        g_free(k);
        skip_space(&p);
        if (*p != ':') return;
        p++;
        skip_space(&p);
        if (want) {
            *value = scan_string(&p);
            if (end) *end = p;
            return;
        }
        if (!skip_value(&p, 0)) return;
        skip_space(&p);
        if (*p == '}') return;
        if (*p != ',') return;
        p++;
    }
}

static gboolean exe_asset_match(const gchar *asset_name, const gchar *tag_version, gpointer user_data) {
    (void)user_data;
    gchar *want = gtv_update_asset_name(tag_version);
    gboolean ok = !strcmp(asset_name, want);
    g_free(want);
    return ok;
}

GtvUpdateStatus gtv_update_parse_release_full(const gchar *json, const gchar *current_version,
                                              GtvAssetMatch match, gpointer match_data,
                                              gchar **out_tag, gchar **out_asset_url) {
    if (out_tag) *out_tag = NULL;
    if (out_asset_url) *out_asset_url = NULL;
    if (!json || !current_version || !match) return GTV_UPDATE_ERROR;
    const gchar *p = json;
    skip_space(&p);
    if (*p != '{') return GTV_UPDATE_ERROR;
    gchar *tag = NULL;
    object_string(p, "tag_name", &tag, NULL);
    if (!tag || !*tag) { g_free(tag); return GTV_UPDATE_ERROR; }
    if (gtv_version_compare(tag, current_version) <= 0) { g_free(tag); return GTV_UPDATE_CURRENT; }
    /* Newer tag: locate the platform asset via the caller matcher. */
    const gchar *tagver = strip_tag(tag);
    gchar *found_url = NULL;
    const gchar *assets = strstr(p, "\"assets\"");
    if (assets) {
        assets = strchr(assets, '[');
        if (assets) {
            const gchar *q = assets + 1;
            for (;;) {
                skip_space(&q);
                if (*q == ']') break;
                if (*q != '{') break;
                const gchar *obj_end = q;
                /* Find matching brace to bound this object. */
                guint depth = 0;
                gboolean in_str = FALSE, esc = FALSE;
                const gchar *r = q;
                for (; *r; r++) {
                    if (in_str) {
                        if (esc) esc = FALSE;
                        else if (*r == '\\') esc = TRUE;
                        else if (*r == '"') in_str = FALSE;
                    } else if (*r == '"') in_str = TRUE;
                    else if (*r == '{') depth++;
                    else if (*r == '}') {
                        depth--;
                        if (depth == 0) { obj_end = r + 1; break; }
                    }
                }
                if (!*r) break;
                gchar *name = NULL, *url = NULL;
                object_string(q, "name", &name, NULL);
                object_string(q, "browser_download_url", &url, NULL);
                if (name && url && match(name, tagver, match_data)) {
                    found_url = url;
                    url = NULL;
                }
                g_free(name);
                g_free(url);
                if (found_url) break;
                q = obj_end;
                skip_space(&q);
                if (*q == ',') q++;
                else if (*q != ']') break;
            }
        }
    }
    if (!found_url) { g_free(tag); return GTV_UPDATE_ERROR; }
    if (out_tag) *out_tag = tag;
    else g_free(tag);
    if (out_asset_url) *out_asset_url = found_url;
    else g_free(found_url);
    return GTV_UPDATE_AVAILABLE;
}

GtvUpdateStatus gtv_update_parse_release(const gchar *json, const gchar *current_version,
                                         gchar **out_tag, gchar **out_asset_url) {
    return gtv_update_parse_release_full(json, current_version, exe_asset_match, NULL,
                                         out_tag, out_asset_url);
}

static gchar *github_api_url(void) {
    const gchar *override = g_getenv("GTV_UPDATE_URL");
    if (override && *override) return g_strdup(override);
    const gchar *repo = g_getenv("GTV_UPDATE_REPO");
    if (!repo || !*repo) repo = GTV_GITHUB_REPO;
    return g_strdup_printf("https://api.github.com/repos/%s/releases/latest", repo);
}

static gchar *curl_get(const gchar *url, glong max_bytes, glong max_secs) {
    gchar *secs = g_strdup_printf("%ld", max_secs);
    gchar *bytes = g_strdup_printf("%ld", max_bytes);
    const gchar *args[] = {"curl", "--silent", "--show-error", "--fail", "--location",
        "--proto", "=https", "--max-time", secs, "--max-filesize", bytes,
        "--header", "Accept: application/vnd.github+json",
        "--header", "User-Agent: GoTiengViet-Updater",
        "--url", url, NULL};
    GSubprocess *proc = g_subprocess_newv(args,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    g_free(secs);
    g_free(bytes);
    if (!proc) return NULL;
    gchar *output = NULL;
    gboolean ok = g_subprocess_communicate_utf8(proc, NULL, NULL, &output, NULL, NULL);
    if (!ok) g_subprocess_force_exit(proc);
    g_subprocess_wait(proc, NULL, NULL);
    if (!ok || !g_subprocess_get_successful(proc)) g_clear_pointer(&output, g_free);
    g_object_unref(proc);
    return output;
}

GtvUpdateStatus gtv_update_check_full(const gchar *repo, const gchar *current_version,
                                 GtvAssetMatch match, gpointer match_data,
                                 gchar **out_tag, gchar **out_asset_url) {
    if (out_tag) *out_tag = NULL;
    if (out_asset_url) *out_asset_url = NULL;
    if (!current_version || !match) return GTV_UPDATE_ERROR;
    gchar *url = NULL;
    if (repo && *repo)
        url = g_strdup_printf("https://api.github.com/repos/%s/releases/latest", repo);
    else
        url = github_api_url();
    gchar *body = curl_get(url, 1048576, 15);
    g_free(url);
    if (!body) return GTV_UPDATE_ERROR;
    GtvUpdateStatus st = gtv_update_parse_release_full(body, current_version, match, match_data,
                                                       out_tag, out_asset_url);
    g_free(body);
    return st;
}

GtvUpdateStatus gtv_update_check(const gchar *repo, const gchar *current_version,
                                 gchar **out_tag, gchar **out_asset_url) {
    return gtv_update_check_full(repo, current_version, exe_asset_match, NULL,
                                 out_tag, out_asset_url);
}

gboolean gtv_update_download(const gchar *url, const gchar *dest_path) {
    if (!url || !dest_path || strncmp(url, "https://", 8)) return FALSE;
    gchar *tmp = g_strdup_printf("%s.part", dest_path);
    const gchar *args[] = {"curl", "--silent", "--show-error", "--fail", "--location",
        "--proto", "=https", "--max-time", "300", "--max-filesize", "157286400",
        "--header", "User-Agent: GoTiengViet-Updater",
        "--output", tmp, "--url", url, NULL};
    GSubprocess *proc = g_subprocess_newv(args,
        G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    if (!proc) { g_free(tmp); return FALSE; }
    gboolean ok = g_subprocess_wait_check(proc, NULL, NULL);
    g_object_unref(proc);
    if (!ok) {
        g_remove(tmp);
        g_free(tmp);
        return FALSE;
    }
    if (g_rename(tmp, dest_path) != 0) {
        g_remove(tmp);
        g_free(tmp);
        return FALSE;
    }
    g_free(tmp);
    return TRUE;
}

static gchar *update_stamp_path(void) {
    return g_build_filename(g_get_user_config_dir(), "gotiengviet", "update-check", NULL);
}

gboolean gtv_update_should_autocheck(void) {
    gchar *path = update_stamp_path();
    gchar *contents = NULL;
    gboolean due = TRUE;
    if (g_file_get_contents(path, &contents, NULL, NULL) && contents) {
        gint64 last = g_ascii_strtoll(g_strstrip(contents), NULL, 10);
        gint64 now = g_get_real_time() / 1000000;
        due = last <= 0 || now - last >= GTV_UPDATE_INTERVAL_SECS;
    }
    g_free(contents);
    g_free(path);
    return due;
}

void gtv_update_mark_checked(void) {
    gchar *path = update_stamp_path();
    gchar *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
    gchar *now = g_strdup_printf("%" G_GINT64_FORMAT, g_get_real_time() / 1000000);
    g_file_set_contents(path, now, -1, NULL);
    g_free(now);
    g_free(path);
}
