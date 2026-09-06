#include "internal.h"
#include <gio/gio.h>

/* curl is already used by the optional Ollama setup. Arguments and JSON are
 * passed directly, never interpolated into a shell command. */
static gchar *ollama_request(const GtvConfig *config, const gchar *endpoint, const gchar *body) {
    gchar *url = g_strconcat(config->url, endpoint, NULL);
    const gchar *args[] = {"curl", "--silent", "--show-error", "--fail", "--max-time", body ? "1.5" : "1",
        "--max-filesize", "1048576", "--proto", "=http,https", "--url", url,
        body ? "--header" : NULL, "Content-Type: application/json", "--data-binary", "@-", NULL};
    GSubprocess *process = g_subprocess_newv(args,
        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    g_free(url);
    if (!process) return NULL;
    gchar *output = NULL;
    if (!g_subprocess_communicate_utf8(process, body, NULL, &output, NULL, NULL) ||
        !g_subprocess_get_successful(process)) g_clear_pointer(&output, g_free);
    g_object_unref(process);
    return output;
}
gboolean gtv_ai_available(const GtvConfig *config) {
    if (!config->ai_enabled) return TRUE; /* Rule provider is always available. */
    gchar *reply = ollama_request(config, "/api/tags", NULL);
    gboolean ok = reply != NULL;
    g_free(reply);
    return ok;
}
GPtrArray *gtv_ai_suggest(const GtvConfig *config, const gchar *word, const gchar *context) {
    gtv_init();
    if (config->ai_enabled && gtv_ai_available(config)) {
        gchar *prompt = g_strdup_printf("Sửa chính tả tiếng Việt: '%s' (ngữ cảnh: '%s') -> chỉ trả về từ đúng, không giải thích:", word, context ? context : "");
        gchar *quoted_prompt = gtv_json_quote(prompt);
        gchar *model = gtv_json_quote(config->model);
        gchar *body = g_strdup_printf("{\"model\":%s,\"prompt\":%s,\"stream\":false}", model, quoted_prompt);
        gchar *reply = ollama_request(config, "/api/generate", body);
        gchar *response = gtv_json_response(reply);
        g_free(reply); g_free(body); g_free(model); g_free(quoted_prompt); g_free(prompt);
        if (response && *g_strstrip(response) && strcmp(response, word)) {
            GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(out, response);
            return out;
        }
        g_free(response);
    }
    if (spell_word_valid(word)) return g_ptr_array_new_with_free_func(g_free);
    return get_suggestions(word);
}

GPtrArray *gtv_predict_next(const GtvConfig *config, const gchar *context, const gchar *prefix) {
    gtv_init();
    if (config && config->ai_enabled && gtv_ai_available(config)) {
        gchar *pfx_hint = (prefix && *prefix) ? g_strdup_printf(" (bắt đầu bằng chữ '%s')", prefix) : g_strdup("");
        gchar *prompt = g_strdup_printf("Dự đoán từ tiếp theo trong tiếng Việt cho ngữ cảnh: '%s'%s -> chỉ trả về tối đa 3 từ cách nhau bằng dấu phẩy, không giải thích:",
            context ? context : "", pfx_hint);
        g_free(pfx_hint);
        gchar *quoted_prompt = gtv_json_quote(prompt);
        gchar *model = gtv_json_quote(config->model);
        gchar *body = g_strdup_printf("{\"model\":%s,\"prompt\":%s,\"stream\":false}", model, quoted_prompt);
        gchar *reply = ollama_request(config, "/api/generate", body);
        gchar *response = gtv_json_response(reply);
        g_free(reply); g_free(body); g_free(model); g_free(quoted_prompt); g_free(prompt);
        if (response && *g_strstrip(response)) {
            GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
            gchar **tokens = g_strsplit_set(response, ",\n", -1);
            for (guint i = 0; tokens[i] && out->len < 5; i++) {
                gchar *stripped = g_strstrip(tokens[i]);
                if (*stripped) add_candidate_unique(out, stripped);
            }
            g_strfreev(tokens);
            g_free(response);
            if (out->len > 0) return out;
            g_ptr_array_unref(out);
        } else {
            g_free(response);
        }
    }
    return gtv_vector_predict_next(context, prefix, 5);
}
