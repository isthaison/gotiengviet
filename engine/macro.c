#include "internal.h"

/* Macros and emojis live in data files, not C arrays (resolved with
 * gtv_data_path: $GTV_DATA_DIR → user config → /usr/share → ./data).
 * Format: key=value per line (split on first '='), '#' comments, blank
 * lines skipped, UTF-8, first entry wins on duplicates. Order is kept for
 * emoji prefix suggestions. Loaded once per process; gtv_tables_reload()
 * forces a reload (tests). */
#define MACRO_FILE "macros.txt"
#define EMOJI_FILE "emojis.txt"
#define TABLE_CAP 2000

typedef struct { gchar *key; gchar *fold; gchar *value; } TableEntry;

static GPtrArray *macro_entries = NULL;
static GPtrArray *emoji_entries = NULL;
static GHashTable *macro_exact = NULL;
static GHashTable *macro_fold = NULL;
static GHashTable *emoji_exact = NULL;
static GHashTable *emoji_fold = NULL;
static gboolean tables_loaded = FALSE;

static void entry_free(gpointer p){
    TableEntry *e = p;
    g_free(e->key); g_free(e->fold); g_free(e->value); g_free(e);
}

static void table_add(GPtrArray *entries, GHashTable *exact, GHashTable *fold,
                      const gchar *key, const gchar *value){
    if(!key || !*key || !value || !*value || entries->len >= TABLE_CAP) return;
    if(g_hash_table_contains(exact, key)) return;
    TableEntry *e = g_new(TableEntry, 1);
    e->key = g_strdup(key);
    e->fold = g_utf8_strdown(key, -1);
    e->value = g_strdup(value);
    g_ptr_array_add(entries, e);
    g_hash_table_insert(exact, e->key, e->value);
    if(!g_hash_table_contains(fold, e->fold))
        g_hash_table_insert(fold, e->fold, e->value);
}

static void table_load_file(GPtrArray *entries, GHashTable *exact, GHashTable *fold,
                            const gchar *path){
    gchar *contents = NULL;
    if(!path || !g_file_get_contents(path, &contents, NULL, NULL)) return;
    gchar **lines = g_strsplit_set(contents, "\r\n", -1);
    for(guint i = 0; lines[i]; i++){
        gchar *line = g_strstrip(lines[i]);
        if(!*line || *line == '#') continue;
        gchar *sep = strchr(line, '=');
        if(!sep) continue;
        *sep = '\0';
        table_add(entries, exact, fold, g_strstrip(line), g_strstrip(sep + 1));
    }
    g_strfreev(lines); g_free(contents);
}

static void tables_init(void){
    macro_entries = g_ptr_array_new_with_free_func(entry_free);
    emoji_entries = g_ptr_array_new_with_free_func(entry_free);
    macro_exact = g_hash_table_new(g_str_hash, g_str_equal);
    macro_fold = g_hash_table_new(g_str_hash, g_str_equal);
    emoji_exact = g_hash_table_new(g_str_hash, g_str_equal);
    emoji_fold = g_hash_table_new(g_str_hash, g_str_equal);
}

static void tables_load(void){
    if(tables_loaded) return;
    tables_loaded = TRUE;
    tables_init();
    gchar *macros = gtv_data_path(MACRO_FILE);
    gchar *emojis = gtv_data_path(EMOJI_FILE);
    table_load_file(macro_entries, macro_exact, macro_fold, macros);
    table_load_file(emoji_entries, emoji_exact, emoji_fold, emojis);
    g_free(macros); g_free(emojis);
}

void gtv_tables_reload(void){
    if(macro_exact) g_hash_table_destroy(macro_exact);
    if(macro_fold) g_hash_table_destroy(macro_fold);
    if(emoji_exact) g_hash_table_destroy(emoji_exact);
    if(emoji_fold) g_hash_table_destroy(emoji_fold);
    macro_exact = macro_fold = emoji_exact = emoji_fold = NULL;
    if(macro_entries) g_ptr_array_unref(macro_entries);
    if(emoji_entries) g_ptr_array_unref(emoji_entries);
    macro_entries = emoji_entries = NULL;
    tables_loaded = FALSE;
    tables_load();
}

/* Trả về chuỗi mới (caller g_free) — đã expand macro/emoji, hoặc bản sao nguyên văn */
gchar* expand_word(const char *word){
    if(!word) return g_strdup("");
    tables_load();
    gpointer v = g_hash_table_lookup(macro_exact, word);
    if(!v){
        gchar *lower = g_utf8_strdown(word, -1);
        v = g_hash_table_lookup(macro_fold, lower);
        g_free(lower);
    }
    if(!v){
        v = g_hash_table_lookup(emoji_exact, word);
        if(!v){
            gchar *lower = g_utf8_strdown(word, -1);
            v = g_hash_table_lookup(emoji_fold, lower);
            g_free(lower);
        }
    }
    return g_strdup(v ? v : word);
}

GPtrArray* get_emoji_suggestions(const char *prefix){
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if(!prefix || !*prefix) return out;
    tables_load();
    gchar *lower = g_utf8_strdown(prefix, -1);
    for(guint i = 0; i < emoji_entries->len && out->len < 5; i++){
        TableEntry *e = emoji_entries->pdata[i];
        if(e->key[0] == ':' && g_str_has_prefix(e->key, lower))
            add_candidate_unique(out, e->value);
    }
    g_free(lower);
    return out;
}
