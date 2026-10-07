/* Bundled + GitHub-updated Vietnamese dictionary.
 *
 * Two layers feed word suggestions, both fully offline:
 *   1. data/dict-vi.txt seed shipped with the app (curated in-repo,
 *      see tools/mkdict.sh), refreshed from the dict-vi.txt asset of
 *      GitHub releases into the user data dir;
 *   2. the user's own learned-words.txt (personal picks win).
 *
 * File format (both copies): UTF-8, '#' comments and blank lines ignored,
 * one word per line, plus an optional `# dict-tag: <release>` header that
 * marks which release published the file. Every entry is re-validated
 * with gtv_word_key() on load, so a corrupt download can never poison
 * suggestions: it is simply ignored.
 */
#include "internal.h"
#include <gio/gio.h>
#include <glib/gstdio.h>

#define GTV_DICT_NAME "dict-vi.txt"
#define GTV_DICT_TAG_PREFIX "# dict-tag:"
typedef struct {
    gchar *word;   /* NFC lowercase display form */
    gchar *folded; /* accent-folded for prefix/edit matching */
} DictEntry;

typedef struct {
    GPtrArray *entries; /* DictEntry*, sorted by folded */
    gchar *path;        /* file actually loaded */
    gchar *tag;         /* dict-tag header ("" when absent) */
    gint64 mtime;
    goffset size;
} DictStore;

static GMutex dict_lock;
static DictStore *dict_cache = NULL;

static void dict_entry_free(gpointer p) {
    DictEntry *e = p;
    if (!e) return;
    g_free(e->word);
    g_free(e->folded);
    g_free(e);
}

static void dict_store_free(DictStore *ds) {
    if (!ds) return;
    if (ds->entries) g_ptr_array_unref(ds->entries);
    g_free(ds->path);
    g_free(ds->tag);
    g_free(ds);
}

static gint entry_cmp_folded(gconstpointer a, gconstpointer b) {
    const DictEntry *ea = *(const DictEntry * const *)a;
    const DictEntry *eb = *(const DictEntry * const *)b;
    return strcmp(ea->folded, eb->folded);
}

/* Parse one dictionary file. Returns entries + tag (both possibly empty).
 * Corrupt lines are skipped, never fatal. */
static DictStore *dict_parse_file(const gchar *path) {
    DictStore *ds = g_new0(DictStore, 1);
    ds->entries = g_ptr_array_new_with_free_func(dict_entry_free);
    ds->path = g_strdup(path);
    ds->tag = g_strdup("");
    gchar *contents = NULL;
    if (!g_file_get_contents(path, &contents, NULL, NULL)) return ds;
    gchar **lines = g_strsplit_set(contents, "\r\n", -1);
    g_free(contents);
    for (guint i = 0; lines[i]; i++) {
        gchar *line = g_strstrip(lines[i]);
        if (!*line) continue;
        if (*line == '#') {
            if (g_str_has_prefix(line, GTV_DICT_TAG_PREFIX)) {
                gchar *tag = g_strstrip((gchar *)line + strlen(GTV_DICT_TAG_PREFIX));
                if (*tag) {
                    g_free(ds->tag);
                    ds->tag = g_strdup(tag);
                }
            }
            continue;
        }
        gchar *key = gtv_word_key(line);
        if (!key) continue;
        DictEntry *e = g_new0(DictEntry, 1);
        e->word = key;
        e->folded = gtv_fold_accents(key);
        if (!e->folded) {
            dict_entry_free(e);
            continue;
        }
        g_ptr_array_add(ds->entries, e);
    }
    g_strfreev(lines);
    /* De-duplicate (same word) then sort by folded for prefix search. */
    g_ptr_array_sort(ds->entries, entry_cmp_folded);
    for (guint i = 1; i < ds->entries->len; ) {
        DictEntry *prev = ds->entries->pdata[i - 1];
        DictEntry *cur = ds->entries->pdata[i];
        if (!strcmp(prev->word, cur->word))
            g_ptr_array_remove_index(ds->entries, i);
        else
            i++;
    }
    return ds;
}

gchar *gtv_dict_user_path(void) {
    return g_build_filename(g_get_user_data_dir(), "gotiengviet", GTV_DICT_NAME, NULL);
}

/* Downloaded copy wins when it parses to a non-empty table; otherwise the
 * shipped seed. A half-written download (crash mid-move) falls back to
 * the seed instead of breaking suggestions. */
static DictStore *dict_load_locked(void) {
    gchar *user = gtv_dict_user_path();
    DictStore *ds = NULL;
    if (g_file_test(user, G_FILE_TEST_EXISTS)) {
        ds = dict_parse_file(user);
        if (ds->entries->len == 0) {
            dict_store_free(ds);
            ds = NULL;
        }
    }
    g_free(user);
    if (!ds) {
        gchar *seed = gtv_data_path(GTV_DICT_NAME);
        ds = dict_parse_file(seed);
        g_free(seed);
    }
    return ds;
}

static gboolean dict_current_locked(const gchar *path, gint64 mtime, goffset size) {
    return dict_cache && dict_cache->path && path &&
        !strcmp(dict_cache->path, path) &&
        dict_cache->mtime == mtime && dict_cache->size == size;
}

/* Process-wide cached table; reloads when the backing file changes
 * (GitHub refresh replaces it under us). Returned pointer is borrowed
 * and valid until the next reload — copy out what you keep. */
const DictStore *gtv_dict_get(void) {
    g_mutex_lock(&dict_lock);
    gchar *user = gtv_dict_user_path();
    const gchar *path = NULL;
    gchar *seed = NULL;
    if (g_file_test(user, G_FILE_TEST_EXISTS)) {
        path = user;
    } else {
        seed = gtv_data_path(GTV_DICT_NAME);
        path = seed;
    }
    GStatBuf st;
    gboolean have_stat = path && g_stat(path, &st) == 0;
    if (!dict_cache || !have_stat ||
        !dict_current_locked(path, (gint64)st.st_mtime, (goffset)st.st_size)) {
        dict_store_free(dict_cache);
        dict_cache = NULL;
        if (have_stat) {
            /* dict_load_locked picks user-first too; path here only
             * decides the freshness check above. */
            dict_cache = dict_load_locked();
            dict_cache->mtime = (gint64)st.st_mtime;
            dict_cache->size = (goffset)st.st_size;
            g_free(dict_cache->path);
            dict_cache->path = g_strdup(path);
        }
    }
    g_free(user);
    g_free(seed);
    const DictStore *out = dict_cache;
    g_mutex_unlock(&dict_lock);
    return out;
}

void gtv_dict_invalidate(void) {
    g_mutex_lock(&dict_lock);
    dict_store_free(dict_cache);
    dict_cache = NULL;
    g_mutex_unlock(&dict_lock);
}

guint gtv_dict_count(void) {
    const DictStore *ds = gtv_dict_get();
    return ds && ds->entries ? ds->entries->len : 0;
}

const gchar *gtv_dict_tag(void) {
    const DictStore *ds = gtv_dict_get();
    return (ds && ds->tag && *ds->tag) ? ds->tag : "";
}

/* Completions: accent-exact prefix first, then accent-folded prefix
 * (thong matches thông), each capped; mirrors gtv_learned_completions. */
void gtv_dict_complete(const gchar *prefix, GPtrArray *out, guint max) {
    if (!prefix || !*prefix || !out || max == 0) return;
    gchar *lower = gtv_word_key(prefix);
    if (!lower) return;
    gchar *folded_prefix = gtv_fold_accents(lower);
    if (!folded_prefix) {
        g_free(lower);
        return;
    }
    const DictStore *ds = gtv_dict_get();
    if (ds && ds->entries) {
        for (guint i = 0; i < ds->entries->len && out->len < max; i++) {
            DictEntry *e = ds->entries->pdata[i];
            if (!strcmp(e->word, lower)) continue;
            if (g_str_has_prefix(e->word, lower))
                add_candidate_unique(out, e->word);
        }
        for (guint i = 0; i < ds->entries->len && out->len < max; i++) {
            DictEntry *e = ds->entries->pdata[i];
            if (!strcmp(e->word, lower)) continue;
            if (!g_str_has_prefix(e->word, lower) &&
                g_str_has_prefix(e->folded, folded_prefix))
                add_candidate_unique(out, e->word);
        }
    }
    g_free(folded_prefix);
    g_free(lower);
}

/* Byte Levenshtein with early exit past cap. Folded forms are lowercase
 * ASCII-ish, so byte distance tracks character distance. */
static guint edit_distance_cap(const gchar *a, const gchar *b, guint cap) {
    gsize na = strlen(a), nb = strlen(b);
    if (na > nb + cap || nb > na + cap) return cap + 1;
    /* Two rows; lengths are short words (<= 64 by gtv_word_key). */
    guint prev[65], cur[65];
    if (na > 64 || nb > 64) return cap + 1;
    for (gsize j = 0; j <= nb; j++) prev[j] = (guint)j;
    for (gsize i = 1; i <= na; i++) {
        cur[0] = (guint)i;
        guint row_min = cur[0];
        for (gsize j = 1; j <= nb; j++) {
            guint cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            guint del = prev[j] + 1, ins = cur[j - 1] + 1, sub = prev[j - 1] + cost;
            guint m = del < ins ? del : ins;
            m = m < sub ? m : sub;
            cur[j] = m;
            if (m < row_min) row_min = m;
        }
        if (row_min > cap) return cap + 1;
        memcpy(prev, cur, (nb + 1) * sizeof(guint));
    }
    return prev[nb];
}

typedef struct {
    const gchar *word;
    guint dist;
    gsize len_diff;
} CorrectCand;

static gint correct_cmp(gconstpointer a, gconstpointer b) {
    const CorrectCand *ca = a, *cb = b;
    if (ca->dist != cb->dist) return ca->dist < cb->dist ? -1 : 1;
    if (ca->len_diff != cb->len_diff) return ca->len_diff < cb->len_diff ? -1 : 1;
    return strcmp(ca->word, cb->word);
}

/* Corrections for a misspelled word: folded edit distance <= 2, ranked by
 * distance, then length closeness, then alphabetically. Accent variants
 * (distance 0, different word: hoa -> hòa) count as candidates. */
void gtv_dict_correct(const gchar *word, GPtrArray *out, guint max) {
    if (!word || !*word || !out || max == 0) return;
    gchar *lower = gtv_word_key(word);
    if (!lower) return;
    gchar *folded = gtv_fold_accents(lower);
    if (!folded) {
        g_free(lower);
        return;
    }
    gsize want_len = strlen(folded);
    const DictStore *ds = gtv_dict_get();
    if (ds && ds->entries) {
        GArray *cands = g_array_new(FALSE, FALSE, sizeof(CorrectCand));
        for (guint i = 0; i < ds->entries->len; i++) {
            DictEntry *e = ds->entries->pdata[i];
            if (!strcmp(e->word, lower)) continue;
            gsize elen = strlen(e->folded);
            gsize diff = elen > want_len ? elen - want_len : want_len - elen;
            if (diff > 2) continue;
            guint d = edit_distance_cap(folded, e->folded, 2);
            if (d <= 2) {
                CorrectCand c = { e->word, d, diff };
                g_array_append_val(cands, c);
            }
        }
        g_array_sort(cands, correct_cmp);
        for (guint i = 0; i < cands->len && out->len < max; i++)
            add_candidate_unique(out, g_array_index(cands, CorrectCand, i).word);
        g_array_unref(cands);
    }
    g_free(folded);
    g_free(lower);
}

static gboolean dict_asset_match(const gchar *asset_name, const gchar *tag_version, gpointer user_data) {
    (void)tag_version;
    (void)user_data;
    return !g_strcmp0(asset_name, GTV_DICT_NAME);
}

/* Refresh the dictionary from the dict-vi.txt asset of the latest GitHub
 * release. Downloads to temp, keeps the file only when its dict-tag header
 * is newer than the loaded table; a corrupt download is ignored and the
 * current table keeps serving. Returns TRUE when the on-disk copy changed
 * (call gtv_dict_invalidate() is unnecessary: the cache keys on mtime). */
gboolean gtv_dict_update_check(const gchar *repo) {
    gchar *tag = NULL, *url = NULL;
    GtvUpdateStatus st = gtv_update_check_full(repo, "0.0.0", dict_asset_match, NULL,
                                               &tag, &url);
    if (st != GTV_UPDATE_AVAILABLE || !url) {
        g_free(tag);
        g_free(url);
        return FALSE;
    }
    gchar *dest = gtv_dict_user_path();
    gchar *tmp = g_strdup_printf("%s.dict-dl", dest);
    /* gtv_update_download appends .part itself and moves the result to
     * tmp; we then promote tmp over the live file only when newer. */
    gboolean ok = gtv_update_download(url, tmp);
    g_free(tag);
    g_free(url);
    if (!ok) {
        g_free(tmp);
        g_free(dest);
        return FALSE;
    }
    /* Validate + compare tags before replacing the live file. */
    DictStore *dl = dict_parse_file(tmp);
    gboolean newer = FALSE;
    if (dl->entries->len > 0 && dl->tag && *dl->tag) {
        const gchar *cur = gtv_dict_tag();
        newer = !*cur || gtv_version_compare(dl->tag, cur) > 0;
    }
    dict_store_free(dl);
    if (newer) {
        gchar *dir = g_path_get_dirname(dest);
        g_mkdir_with_parents(dir, 0700);
        g_free(dir);
        if (g_rename(tmp, dest) == 0) {
            g_free(tmp);
            g_free(dest);
            return TRUE;
        }
    }
    g_remove(tmp);
    g_free(tmp);
    g_free(dest);
    return FALSE;
}
