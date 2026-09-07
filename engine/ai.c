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
static gchar *clean_token(const gchar *raw) {
    if (!raw) return NULL;
    const gchar *p = raw;
    while (*p == '-' || *p == '*' || *p == '#' || g_ascii_isdigit(*p) || *p == '.' || *p == ')' || *p == ' ' || *p == '\t' || *p == '"' || *p == '\'') p++;
    gchar *res = g_strdup(p);
    g_strstrip(res);
    glong len = strlen(res);
    while (len > 0 && (res[len-1] == '"' || res[len-1] == '\'' || res[len-1] == '.' || res[len-1] == ',')) {
        res[--len] = '\0';
    }
    return res;
}

GPtrArray *gtv_ai_suggest(const GtvConfig *config, const gchar *word, const gchar *context) {
    gtv_init();
    if (config && config->ai_enabled && gtv_ai_available(config)) {
        gchar *prompt = g_strdup_printf("Sửa chính tả tiếng Việt: '%s' (ngữ cảnh: '%s') -> chỉ trả về từ đúng, không giải thích:", word, context ? context : "");
        gchar *quoted_prompt = gtv_json_quote(prompt);
        gchar *model = gtv_json_quote(config->model);
        gchar *body = g_strdup_printf("{\"model\":%s,\"prompt\":%s,\"stream\":false,\"options\":{\"num_predict\":20,\"temperature\":0.1}}", model, quoted_prompt);
        gchar *reply = ollama_request(config, "/api/generate", body);
        gchar *response = gtv_json_response(reply);
        g_free(reply); g_free(body); g_free(model); g_free(quoted_prompt); g_free(prompt);
        if (response && *g_strstrip(response) && strcmp(response, word)) {
            gchar *cleaned = clean_token(response);
            g_free(response);
            if (cleaned && *cleaned && strcmp(cleaned, word)) {
                GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
                g_ptr_array_add(out, cleaned);
                return out;
            }
            g_free(cleaned);
        } else {
            g_free(response);
        }
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
        gchar *body = g_strdup_printf("{\"model\":%s,\"prompt\":%s,\"stream\":false,\"options\":{\"num_predict\":20,\"temperature\":0.1}}", model, quoted_prompt);
        gchar *reply = ollama_request(config, "/api/generate", body);
        gchar *response = gtv_json_response(reply);
        g_free(reply); g_free(body); g_free(model); g_free(quoted_prompt); g_free(prompt);
        if (response && *g_strstrip(response)) {
            GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
            gchar **tokens = g_strsplit_set(response, ",\n", -1);
            for (guint i = 0; tokens[i] && out->len < 5; i++) {
                gchar *cleaned = clean_token(tokens[i]);
                if (cleaned && *cleaned) {
                    add_candidate_unique(out, cleaned);
                }
                g_free(cleaned);
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

GPtrArray *gtv_suggest_combined(const GtvConfig *config, const gchar *context, const gchar *preedit, gboolean bad) {
    gtv_init();
    GPtrArray *sugs = g_ptr_array_new_with_free_func(g_free);
    if (!preedit || !*preedit) return sugs;

    if (bad) {
        GPtrArray *corrections = gtv_ai_suggest(config, preedit, context);
        for (guint i = 0; i < corrections->len && sugs->len < 5; i++) {
            add_candidate_unique(sugs, g_ptr_array_index(corrections, i));
        }
        g_ptr_array_unref(corrections);
    }

    if (sugs->len < 5) {
        GPtrArray *predictions = gtv_predict_next(config, context, preedit);
        for (guint i = 0; i < predictions->len && sugs->len < 5; i++) {
            add_candidate_unique(sugs, g_ptr_array_index(predictions, i));
        }
        g_ptr_array_unref(predictions);
    }

    return sugs;
}

typedef struct {
    GtvConfig config;
    gchar *context;
    gchar *preedit;
    gboolean bad;
} SuggestTaskData;

static void suggest_task_data_free(gpointer data) {
    SuggestTaskData *d = data;
    if (!d) return;
    gtv_config_clear(&d->config);
    g_free(d->context);
    g_free(d->preedit);
    g_free(d);
}

static void suggest_worker_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
    SuggestTaskData *d = task_data;
    if (g_task_return_error_if_cancelled(task)) return;
    GPtrArray *results = gtv_suggest_combined(&d->config, d->context, d->preedit, d->bad);
    if (g_task_return_error_if_cancelled(task)) {
        g_ptr_array_unref(results);
        return;
    }
    g_task_return_pointer(task, results, (GDestroyNotify)g_ptr_array_unref);
}

void gtv_suggest_combined_async(const GtvConfig *config, const gchar *context, const gchar *preedit, gboolean bad,
                               GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    GTask *task = g_task_new(NULL, cancellable, callback, user_data);
    SuggestTaskData *d = g_new0(SuggestTaskData, 1);
    if (config) {
        d->config = (GtvConfig){
            .mode = config->mode,
            .modern = config->modern,
            .spellcheck = config->spellcheck,
            .ai_enabled = config->ai_enabled,
            .model = g_strdup(config->model),
            .url = g_strdup(config->url),
            .port = g_strdup(config->port)
        };
    }
    d->context = g_strdup(context);
    d->preedit = g_strdup(preedit);
    d->bad = bad;
    g_task_set_task_data(task, d, suggest_task_data_free);
    g_task_run_in_thread(task, suggest_worker_thread);
    g_object_unref(task);
}

GPtrArray *gtv_suggest_combined_finish(GAsyncResult *res, GError **error) {
    g_return_val_if_fail(g_task_is_valid(res, NULL), NULL);
    return g_task_propagate_pointer(G_TASK(res), error);
}

static void predict_worker_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
    SuggestTaskData *d = task_data;
    if (g_task_return_error_if_cancelled(task)) return;
    GPtrArray *results = gtv_predict_next(&d->config, d->context, d->preedit);
    if (g_task_return_error_if_cancelled(task)) {
        g_ptr_array_unref(results);
        return;
    }
    g_task_return_pointer(task, results, (GDestroyNotify)g_ptr_array_unref);
}

void gtv_predict_next_async(const GtvConfig *config, const gchar *context, const gchar *prefix,
                            GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
    GTask *task = g_task_new(NULL, cancellable, callback, user_data);
    SuggestTaskData *d = g_new0(SuggestTaskData, 1);
    if (config) {
        d->config = (GtvConfig){
            .mode = config->mode,
            .modern = config->modern,
            .spellcheck = config->spellcheck,
            .ai_enabled = config->ai_enabled,
            .model = g_strdup(config->model),
            .url = g_strdup(config->url),
            .port = g_strdup(config->port)
        };
    }
    d->context = g_strdup(context);
    d->preedit = g_strdup(prefix);
    g_task_set_task_data(task, d, suggest_task_data_free);
    g_task_run_in_thread(task, predict_worker_thread);
    g_object_unref(task);
}

GPtrArray *gtv_predict_next_finish(GAsyncResult *res, GError **error) {
    g_return_val_if_fail(g_task_is_valid(res, NULL), NULL);
    return g_task_propagate_pointer(G_TASK(res), error);
}
