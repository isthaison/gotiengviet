#include "internal.h"
#include <gio/gio.h>

#ifdef _WIN32
#include <windows.h>

static gchar *ollama_request_win32(const GtvConfig *config, const gchar *endpoint, const gchar *body, GCancellable *cancel) {
    (void)cancel;
    gchar *url = g_strconcat(config->url, endpoint, NULL);
    HANDLE hStdInRead = NULL, hStdInWrite = NULL;
    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (body) {
        if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0)) {
            g_free(url);
            return NULL;
        }
        SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);
    }
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        if (hStdInRead) { CloseHandle(hStdInRead); CloseHandle(hStdInWrite); }
        g_free(url);
        return NULL;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = body ? hStdInRead : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;
    ZeroMemory(&pi, sizeof(pi));

    gunichar2 *wurl = g_utf8_to_utf16(url, -1, NULL, NULL, NULL);
    g_free(url);
    if (!wurl) {
        if (hStdInRead) { CloseHandle(hStdInRead); CloseHandle(hStdInWrite); }
        CloseHandle(hStdOutRead); CloseHandle(hStdOutWrite);
        return NULL;
    }

    const char *max_time = body ? "20" : "1";
    gchar *curl_path = NULL;
    const gchar *test_dir = g_getenv("GTV_TEST_CURL_DIR");
    if (test_dir && *test_dir) {
        gchar *cand = g_build_filename(test_dir, "curl.exe", NULL);
        if (g_file_test(cand, G_FILE_TEST_EXISTS)) curl_path = cand;
        else g_free(cand);
    }
    if (!curl_path) curl_path = g_find_program_in_path("curl.exe");
    if (!curl_path) curl_path = g_find_program_in_path("curl");
    gunichar2 *wprog = curl_path ? g_utf8_to_utf16(curl_path, -1, NULL, NULL, NULL) : NULL;
    g_free(curl_path);
    const WCHAR *prog_cmd = wprog ? (LPCWSTR)wprog : L"curl.exe";

    size_t cmdlen = 0;
    { gunichar2 *p = wurl; while (*p++) cmdlen++; }
    cmdlen += wcslen(prog_cmd) + 256;
    WCHAR *cmd = g_new0(WCHAR, cmdlen);
    if (body) {
        wsprintfW(cmd, L"\"%ls\" --silent --show-error --fail --max-time %hs --max-filesize 1048576 --proto =http,https --header \"Content-Type: application/json\" --data-binary @- --url \"%ls\"", prog_cmd, max_time, (LPCWSTR)wurl);
    } else {
        wsprintfW(cmd, L"\"%ls\" --silent --show-error --fail --max-time %hs --max-filesize 1048576 --proto =http,https --url \"%ls\"", prog_cmd, max_time, (LPCWSTR)wurl);
    }
    g_free(wurl);
    g_free(wprog);

    BOOL created = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    g_free(cmd);
    if (hStdInRead) CloseHandle(hStdInRead);
    CloseHandle(hStdOutWrite);

    if (!created) {
        if (hStdInWrite) CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        return NULL;
    }

    if (body && hStdInWrite) {
        DWORD written = 0;
        size_t len = strlen(body);
        WriteFile(hStdInWrite, body, (DWORD)len, &written, NULL);
        CloseHandle(hStdInWrite);
    }

    CloseHandle(pi.hThread);
    DWORD timeout = body ? 25000 : 3000;
    DWORD wr = WaitForSingleObject(pi.hProcess, timeout);
    if (wr == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
    }
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    GString *res = g_string_new("");
    char buf[1024];
    DWORD bytesRead = 0;
    while (ReadFile(hStdOutRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        g_string_append(res, buf);
    }
    CloseHandle(hStdOutRead);

    if (exit_code != 0) {
        g_string_free(res, TRUE);
        return NULL;
    }
    return g_string_free(res, FALSE);
}
#endif

/* Requests run on a worker; cancellation also stops the curl process. */
static gchar *ollama_request(const GtvConfig *config, const gchar *endpoint, const gchar *body, GCancellable *cancel) {
#ifdef _WIN32
    return ollama_request_win32(config, endpoint, body, cancel);
#else
    gchar *url = g_strconcat(config->url, endpoint, NULL);
    const gchar *args[] = {"curl", "--silent", "--show-error", "--fail", "--max-time", body ? "20" : "1",
        "--max-filesize", "1048576", "--proto", "=http,https", "--url", url,
        body ? "--header" : NULL, "Content-Type: application/json", "--data-binary", "@-", NULL};
    GSubprocess *process = g_subprocess_newv(args,
        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    g_free(url);
    if (!process) return NULL;
    gchar *output = NULL;
    gboolean ok=g_subprocess_communicate_utf8(process, body, cancel, &output, NULL, NULL);
    if (!ok){g_subprocess_force_exit(process);g_subprocess_wait(process,NULL,NULL);}
    if (!ok || !g_subprocess_get_successful(process)) g_clear_pointer(&output, g_free);
    g_object_unref(process);
    return output;
#endif
}
gboolean gtv_ai_available(const GtvConfig *config) {
    if (!config || !config->ai_enabled) return FALSE;
    gchar *reply = ollama_request(config, "/api/tags", NULL, NULL);
    gboolean ok = reply != NULL;g_free(reply);return ok;
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

gchar *gtv_fold_accents(const gchar *text){
    gchar *lower=g_utf8_strdown(text,-1),*decomposed=g_utf8_normalize(lower,-1,G_NORMALIZE_NFD);
    GString *folded=g_string_new("");
    for(const gchar *p=decomposed;*p;p=g_utf8_next_char(p)){
        gunichar c=g_utf8_get_char(p);
        if(!g_unichar_ismark(c))g_string_append_unichar(folded,c==0x0111 ? 'd' : c);
    }
    g_free(lower);g_free(decomposed);return g_string_free(folded,FALSE);
}
static GPtrArray *suggest(const GtvConfig *config,const gchar *context,const gchar *prefix,gboolean bad,GCancellable *cancel){
    GPtrArray *out=g_ptr_array_new_with_free_func(g_free);
    if(!config || !config->ai_enabled || !config->url || !config->model ||
       (prefix && !g_utf8_validate(prefix,-1,NULL)) || (context && !g_utf8_validate(context,-1,NULL)))return out;
    gchar *templ=gtv_prompt_get("suggest","prompt",NULL);
    gchar *hint=gtv_prompt_get("suggest",bad ? "correct_hint" : "complete_hint","");
    if(!templ){g_free(hint);return out;}
    const gchar *pargs[]={context ? context : "",prefix ? prefix : "",hint};
    gchar *prompt=gtv_format_template(templ,pargs,3);
    g_free(templ);g_free(hint);
    gchar *quoted=gtv_json_quote(prompt),*model=gtv_json_quote(config->model);
    gchar *body=g_strdup_printf("{\"model\":%s,\"prompt\":%s,\"stream\":false,\"keep_alive\":\"5m\",\"options\":{\"num_predict\":40,\"temperature\":0}}",model,quoted);
    gchar *reply=ollama_request(config,"/api/generate",body,cancel),*response=gtv_json_response(reply);
    if(response && g_utf8_validate(response,-1,NULL)){
        gchar **tokens=g_strsplit_set(response,",\n",-1);
        for(guint i=0;tokens[i] && out->len<3;i++){
            gchar *word=clean_token(tokens[i]);
            if(word && prefix && *prefix && g_unichar_islower(g_utf8_get_char(prefix))){
                gchar *lower=g_utf8_strdown(word,-1);g_free(word);word=lower;
            }
            if(word && *word && g_utf8_strlen(word,-1)<=48 && !strchr(word,':') && g_strcmp0(word,prefix)){
                gchar *folded=gtv_fold_accents(word),*typed=gtv_fold_accents(prefix ? prefix : "");
                if(bad || !*typed || g_str_has_prefix(folded,typed))add_candidate_unique(out,word);
                g_free(folded);g_free(typed);
            }
            g_free(word);
        }
        g_strfreev(tokens);
    }
    g_free(response);g_free(reply);g_free(body);g_free(model);g_free(quoted);g_free(prompt);
    return out;
}
GPtrArray *gtv_ai_suggest(const GtvConfig *config,const gchar *word,const gchar *context){return suggest(config,context,word,TRUE,NULL);}
GPtrArray *gtv_predict_next(const GtvConfig *config,const gchar *context,const gchar *prefix){return suggest(config,context,prefix,FALSE,NULL);}
GPtrArray *gtv_suggest_combined(const GtvConfig *config,const gchar *context,const gchar *preedit,gboolean bad){return suggest(config,context,preedit,bad,NULL);}

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
    GPtrArray *results = suggest(&d->config, d->context, d->preedit, d->bad, cancellable);
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
