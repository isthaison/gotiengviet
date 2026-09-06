/* Small JSON codec for Ollama's non-streaming response. No external runtime. */
#include "internal.h"

static void skip_space(const gchar **p) {
    while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') (*p)++;
}
static gboolean hex4(const gchar **p, gunichar *value) {
    *value = 0;
    for (int i = 0; i < 4; i++) {
        int digit = g_ascii_xdigit_value(**p);
        if (digit < 0) return FALSE;
        *value = (*value << 4) | digit;
        (*p)++;
    }
    return TRUE;
}
static gchar *read_string(const gchar **p) {
    if (*(*p)++ != '"') return NULL;
    GString *out = g_string_new("");
    while (**p && **p != '"') {
        guchar c = *(*p)++;
        if (c < 0x20) goto invalid;
        if (c != '\\') { g_string_append_c(out, c); continue; }
        gchar escape = **p;
        if (!escape) goto invalid;
        (*p)++;
        switch (escape) {
        case '"': case '\\': case '/': g_string_append_c(out, escape); break;
        case 'b': g_string_append_c(out, '\b'); break;
        case 'f': g_string_append_c(out, '\f'); break;
        case 'n': g_string_append_c(out, '\n'); break;
        case 'r': g_string_append_c(out, '\r'); break;
        case 't': g_string_append_c(out, '\t'); break;
        case 'u': {
            gunichar code;
            if (!hex4(p, &code)) goto invalid;
            if (code >= 0xd800 && code <= 0xdbff) {
                gunichar low;
                if (**p != '\\' || (*p)[1] != 'u') goto invalid;
                *p += 2;
                if (!hex4(p, &low) || low < 0xdc00 || low > 0xdfff) goto invalid;
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
            }
            if (!code || !g_unichar_validate(code)) goto invalid;
            g_string_append_unichar(out, code);
            break;
        }
        default: goto invalid;
        }
    }
    if (**p != '"' || !g_utf8_validate(out->str, out->len, NULL)) goto invalid;
    (*p)++;
    return g_string_free(out, FALSE);
invalid:
    g_string_free(out, TRUE);
    return NULL;
}
static gboolean read_value(const gchar **p, guint depth, gchar **response) {
    if (depth > 64) return FALSE;
    skip_space(p);
    if (**p == '"') {
        gchar *value = read_string(p);
        gboolean ok = value != NULL;
        g_free(value);
        return ok;
    }
    if (**p == '{' || **p == '[') {
        gboolean object = **p == '{';
        gchar close = object ? '}' : ']';
        (*p)++; skip_space(p);
        if (**p == close) { (*p)++; return TRUE; }
        while (**p) {
            gboolean capture = FALSE;
            if (object) {
                if (**p != '"') return FALSE;
                gchar *key = read_string(p);
                if (!key) return FALSE;
                capture = depth == 0 && !strcmp(key, "response");
                g_free(key); skip_space(p);
                if (**p != ':') return FALSE;
                (*p)++; skip_space(p);
            }
            if (capture) {
                g_clear_pointer(response, g_free);
                if (**p == '"') {
                    *response = read_string(p);
                    if (!*response) return FALSE;
                } else if (!read_value(p, depth + 1, response)) return FALSE;
            } else if (!read_value(p, depth + 1, response)) return FALSE;
            skip_space(p);
            if (**p == close) { (*p)++; return TRUE; }
            if (**p != ',') return FALSE;
            (*p)++; skip_space(p);
        }
        return FALSE;
    }
    const gchar *literals[] = {"true", "false", "null"};
    for (guint i = 0; i < G_N_ELEMENTS(literals); i++) {
        if (g_str_has_prefix(*p, literals[i])) { *p += strlen(literals[i]); return TRUE; }
    }
    if (**p == '-') (*p)++;
    if (**p == '0') (*p)++;
    else {
        if (**p < '1' || **p > '9') return FALSE;
        while (g_ascii_isdigit(**p)) (*p)++;
    }
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
gchar *gtv_json_response(const gchar *json) {
    if (!json) return NULL;
    const gchar *p = json;
    skip_space(&p);
    if (*p != '{') return NULL;
    gchar *response = NULL;
    gboolean ok = read_value(&p, 0, &response);
    skip_space(&p);
    if (!ok || *p) g_clear_pointer(&response, g_free);
    return response;
}
gchar *gtv_json_quote(const gchar *text) {
    GString *out = g_string_new("\"");
    for (const guchar *p = (const guchar *)text; *p; p++) {
        if (*p == '"' || *p == '\\') g_string_append_c(out, '\\');
        if (*p < 0x20) g_string_append_printf(out, "\\u%04x", *p);
        else g_string_append_c(out, *p);
    }
    g_string_append_c(out, '"');
    return g_string_free(out, FALSE);
}
