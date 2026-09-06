#include "internal.h"

void gtv_init(void) {
    static gsize initialized;
    if (g_once_init_enter(&initialized)) {
        init_charset();
        g_once_init_leave(&initialized, 1);
    }
}

gchar *gtv_transform(const gchar *input, GtvMode mode, gboolean modern) {
    gtv_init();
    if (!input || !g_utf8_validate(input, -1, NULL)) return NULL;
    GArray *buf = g_array_new(FALSE, FALSE, sizeof(gunichar));
    for (const gchar *p = input; *p; p = g_utf8_next_char(p)) {
        gunichar key = g_utf8_get_char(p);
        gtv_compose(buf, key, mode, modern);
    }
    gchar *result = buf->len ? g_ucs4_to_utf8((gunichar *)buf->data, buf->len, NULL, NULL, NULL) : g_strdup("");
    g_array_unref(buf);
    return result;
}

GtvEngine *gtv_engine_new(const GtvConfig *config) {
    gtv_init();
    GtvEngine *engine = g_new0(GtvEngine, 1);
    engine->mode = config->mode;
    engine->modern = config->modern;
    engine->spellcheck = config->spellcheck;
    engine->buffer = g_array_new(FALSE, FALSE, sizeof(gunichar));
    engine->suggestions = g_ptr_array_new_with_free_func(g_free);
    return engine;
}
void gtv_engine_free(GtvEngine *engine) {
    if (!engine) return;
    g_array_unref(engine->buffer);
    g_ptr_array_unref(engine->suggestions);
    g_free(engine);
}
void gtv_engine_reset(GtvEngine *engine) { g_array_set_size(engine->buffer, 0); }
gchar *gtv_engine_buffer(const GtvEngine *engine) {
    return engine->buffer->len ? g_ucs4_to_utf8((gunichar *)engine->buffer->data, engine->buffer->len, NULL, NULL, NULL) : g_strdup("");
}
gchar *gtv_engine_process(GtvEngine *engine, gunichar key, guint *backspaces) {
    if (backspaces) *backspaces = 0;
    GArray *buf = engine->buffer;
    if (key == '\b' || key == 127) {
        if (buf->len) {
            g_array_set_size(buf, buf->len - 1);
            if (backspaces) *backspaces = 1;
        }
        return NULL;
    }
    gboolean shortcut_key = engine->mode == GTV_TELEX && (key == '[' || key == ']' || key == '{' || key == '}');
    gboolean is_emoji_seq = FALSE;
    if (buf->len == 0 && (key == ':' || key == ';' || key == '<')) {
        is_emoji_seq = TRUE;
    } else if (buf->len > 0 && g_array_index(buf, gunichar, 0) == ':' && key == ':') {
        is_emoji_seq = TRUE;
    } else if (buf->len == 1 && (g_array_index(buf, gunichar, 0) == ':' || g_array_index(buf, gunichar, 0) == ';') && key == '-') {
        is_emoji_seq = TRUE;
    }
    if (!shortcut_key && !is_emoji_seq && (g_unichar_isspace(key) || g_unichar_ispunct(key) || (g_unichar_isdefined(key) && !g_unichar_isalnum(key) && !g_unichar_ismark(key)))) {
        gchar *word = gtv_engine_buffer(engine);
        GString *combo = g_string_new(word);
        g_string_append_unichar(combo, key);
        gchar *combo_expanded = expand_word(combo->str);
        if (strcmp(combo->str, combo_expanded) != 0) {
            guint old_len = buf->len;
            g_free(word);
            g_string_free(combo, TRUE);
            gtv_engine_reset(engine);
            if (backspaces) *backspaces = old_len;
            return combo_expanded;
        }
        g_free(combo_expanded);
        g_string_free(combo, TRUE);

        gchar *expanded = expand_word(word);
        g_free(word);
        g_ptr_array_set_size(engine->suggestions, 0);
        if (engine->spellcheck && *expanded && !spell_word_valid(expanded)) {
            g_ptr_array_unref(engine->suggestions);
            engine->suggestions = get_suggestions(expanded);
        }
        GString *commit = g_string_new(expanded);
        g_string_append_unichar(commit, key);
        g_free(expanded);
        gtv_engine_reset(engine);
        return g_string_free(commit, FALSE);
    }
    guint old_len = buf->len;
    gchar *before = gtv_engine_buffer(engine);
    gtv_compose(buf, key, engine->mode, engine->modern);
    gchar *current = gtv_engine_buffer(engine);
    gchar *expanded = expand_word(current);
    if (strcmp(current, expanded) != 0 && (key == ':' || key == ')' || key == 'D' || key == 'P' || key == 'p' || key == '3' || key == '>')) {
        g_free(current);
        gtv_engine_reset(engine);
        if (backspaces) *backspaces = old_len;
        g_free(before);
        return expanded;
    }
    g_free(current);
    g_free(expanded);
    if (backspaces) {
        gchar *after = gtv_engine_buffer(engine);
        GString *literal = g_string_new(before);
        g_string_append_unichar(literal, key);
        if (strcmp(literal->str, after)) *backspaces = old_len;
        g_string_free(literal, TRUE);
        g_free(after);
    }
    g_free(before);
    return NULL;
}
