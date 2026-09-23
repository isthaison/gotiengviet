/* Inline keyword suggestions for the Windows TSF service.
 *
 * Broker architecture (system-wide, Electron-safe): this DLL never creates
 * windows inside the host process. Candidates (emoji instantly, Ollama after
 * a 350 ms debounce, offline learned data as fallback) are forwarded to the
 * tray process overlay via WM_COPYDATA (windows/suggest_ipc.h); the tray
 * draws them near the caret without stealing focus. Selection keys mirror
 * IBus/Linux and are handled here: Up/Down navigate, 1-5 accept (Telex
 * only), Tab accepts the highlight (exact macro/emoji match wins first),
 * Enter commits, Esc dismisses, Space commits literally.
 *
 * Threading: key events run on the host app thread; only the Ollama fetch
 * runs on a worker (synchronous engine API + GCancellable with a sleep
 * debounce). Shared state is CS-guarded. Stale results are dropped by
 * generation counter; the tray also drops out-of-order payloads per source.
 * No caret rect -> no overlay (never fall back to a screen corner).
 */
#include "tsf_suggest.h"
#include "tsf_service.h"
extern "C" {
#include "internal.h"
}
#include "../app.h"
#include "../suggest_ipc.h"
#include <new>

#define GTV_SUGGEST_MAX 5
#define GTV_SUGGEST_DEBOUNCE_MS 350

typedef struct {
    gchar *text;
    gboolean from_ai;
} SuggestItem;

typedef struct {
    CRITICAL_SECTION cs; /* guards shown, cursor, caret, generation, cancellable, tables */
    GPtrArray *shown;    /* SuggestItem */
    gint cursor;
    gchar *pending_query; /* app thread only */
    gboolean pending_bad;
    guint generation;
    RECT caret;          /* captured on the app thread at trigger time */
    gboolean caret_valid;
    RECT last_caret;
    gboolean last_caret_valid;
    GCancellable *cancellable;
    GHashTable *learned_words;
    GHashTable *learned_fixes;
} SuggestState;

static SuggestState g_suggest;
static LONG g_suggest_init = 0;

static void suggest_init_once(void) {
    if (InterlockedCompareExchange(&g_suggest_init, 1, 0) != 0) return;
    InitializeCriticalSection(&g_suggest.cs);
    /* Items own both their struct and text; free_items() releases them.
     * Do not give the array a free func too, or clearing double-frees
     * every candidate and can terminate the TSF host process. */
    g_suggest.shown = g_ptr_array_new();
}

static void free_items(GPtrArray *arr) {
    if (!arr) return;
    for (guint i = 0; i < arr->len; i++) {
        SuggestItem *it = (SuggestItem *)g_ptr_array_index(arr, i);
        g_free(it->text);
        g_free(it);
    }
    g_ptr_array_set_size(arr, 0);
}

static void learned_ensure_locked(void) {
    if (!g_suggest.learned_words) {
        g_suggest.learned_words = gtv_words_table_new();
        gchar *p = gtv_learned_path("learned-words.txt");
        gtv_words_load(g_suggest.learned_words, p);
        g_free(p);
    }
    if (!g_suggest.learned_fixes) {
        g_suggest.learned_fixes = gtv_fixes_table_new();
        gchar *p = gtv_learned_path("learned-corrections.txt");
        gtv_fixes_load(g_suggest.learned_fixes, p);
        g_free(p);
    }
}

/* Mirror of the tray learning: accepted corrections teach offline data. */
static void learn_accepted_locked(const gchar *typed, const gchar *fix) {
    if (!typed || !*typed || !fix || !*fix) return;
    learned_ensure_locked();
    gboolean dirty_fix = gtv_fixes_learn(g_suggest.learned_fixes, typed, fix);
    gboolean dirty_words = FALSE;
    gchar **words = g_strsplit_set(fix, " \t", -1);
    for (guint i = 0; words[i]; i++) {
        if (gtv_words_learn(g_suggest.learned_words, words[i]))
            dirty_words = TRUE;
    }
    g_strfreev(words);
    if (dirty_fix) {
        gchar *p = gtv_learned_path("learned-corrections.txt");
        gtv_fixes_save(g_suggest.learned_fixes, p);
        g_free(p);
    }
    if (dirty_words) {
        gchar *p = gtv_learned_path("learned-words.txt");
        gtv_words_save(g_suggest.learned_words, p);
        g_free(p);
    }
}

static gboolean suggest_word_known(const gchar *word) {
    if (!word || !*word) return FALSE;
    if (spell_word_valid(word)) return TRUE;
    EnterCriticalSection(&g_suggest.cs);
    learned_ensure_locked();
    gboolean hit = g_hash_table_contains(g_suggest.learned_words, word);
    LeaveCriticalSection(&g_suggest.cs);
    return hit;
}

/* Report a committed candidate to the tray (same channel the key sink
 * uses for typed words, so balloons/learning stay consistent). */
static void notify_tray_word(const char *word) {
    if (!word || !*word) return;
    HWND tray = FindWindowA(GTV_TRAY_WINDOW_CLASS, NULL);
    if (!tray) return;
    COPYDATASTRUCT cds;
    cds.dwData = (ULONG_PTR)GTV_AI_COPYDATA_ID;
    cds.cbData = (DWORD)strlen(word) + 1;
    cds.lpData = (PVOID)word;
    SendMessageTimeoutA(tray, WM_COPYDATA, 0, (LPARAM)&cds,
                        SMTO_ABORTIFHUNG, 500, NULL);
}

/* -- caret rectangle -------------------------------------------------- */

BOOL CGtvTextService::GetCaretRect(ITfContext *pic, RECT *rc) {
    if (!pic || !rc) return FALSE;
    /* Read-only caret probe: open a tiny sync session through the context.
     * Implemented with a one-shot edit session object below. */
    class CCaretSession : public ITfEditSession {
    public:
        CCaretSession(CGtvTextService *service, ITfContext *p, RECT *o, BOOL *ok)
            : m_cRef(1), m_service(service), m_pic(p), m_out(o), m_ok(ok) {
            if (m_service) m_service->AddRef();
            if (m_pic) m_pic->AddRef();
        }
        virtual ~CCaretSession() {
            if (m_pic) m_pic->Release();
            if (m_service) m_service->Release();
        }
        STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
            if (!ppv) return E_INVALIDARG;
            if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                *ppv = static_cast<ITfEditSession *>(this);
                AddRef();
                return S_OK;
            }
            *ppv = NULL;
            return E_NOINTERFACE;
        }
        STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
        STDMETHODIMP_(ULONG) Release() {
            LONG c = InterlockedDecrement(&m_cRef);
            if (c == 0) delete this;
            return c;
        }
        STDMETHODIMP DoEditSession(TfEditCookie ec) {
            ITfContextView *view = NULL;
            if (FAILED(m_pic->GetActiveView(&view)) || !view)
                return E_FAIL;

            BOOL got = FALSE;
            RECT r = {0, 0, 0, 0};
            BOOL clipped = FALSE;

            /* 1. Try active composition range first. Essential for Chromium/Electron/
             * OpenCode/VSCode because querying a 0-length collapsed selection often
             * yields TS_E_NOLAYOUT or E_FAIL before reflow. The composition range has
             * real character bounds. */
            if (m_service && m_service->m_pComposition) {
                ITfRange *compRange = NULL;
                if (SUCCEEDED(m_service->m_pComposition->GetRange(&compRange)) && compRange) {
                    if (SUCCEEDED(view->GetTextExt(ec, compRange, &r, &clipped)) &&
                        (r.right > r.left || r.bottom > r.top || r.left != 0 || r.top != 0)) {
                        if (r.right > r.left) r.left = r.right;
                        got = TRUE;
                    }
                    compRange->Release();
                }
            }

            /* 2. Try selection range if composition range was not available or failed */
            if (!got) {
                TF_SELECTION sel;
                ULONG fetched = 0;
                if (SUCCEEDED(m_pic->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) &&
                    fetched == 1 && sel.range) {
                    if (SUCCEEDED(view->GetTextExt(ec, sel.range, &r, &clipped)) &&
                        (r.right > r.left || r.bottom > r.top || r.left != 0 || r.top != 0)) {
                        got = TRUE;
                    }
                    sel.range->Release();
                }
            }

            view->Release();

            if (got && m_out && m_ok) {
                *m_out = r;
                *m_ok = TRUE;
            }
            return S_OK;
        }

    private:
        volatile LONG m_cRef;
        CGtvTextService *m_service;
        ITfContext *m_pic;
        RECT *m_out;
        BOOL *m_ok;
    };

    RECT out = {0, 0, 0, 0};
    BOOL ok = FALSE;
    CCaretSession *s = new (std::nothrow) CCaretSession(this, pic, &out, &ok);
    if (s) {
        HRESULT hrSession = E_FAIL;
        pic->RequestEditSession(m_tfClientId, s, TF_ES_SYNC | TF_ES_READ, &hrSession);
        s->Release();
    }

    /* Fallback 1: Win32 caret via GetGUIThreadInfo */
    if (!ok) {
        GUITHREADINFO gti;
        memset(&gti, 0, sizeof(gti));
        gti.cbSize = sizeof(gti);
        if (GetGUIThreadInfo(0, &gti)) {
            if (gti.hwndCaret && IsWindow(gti.hwndCaret) &&
                (gti.rcCaret.right > gti.rcCaret.left || gti.rcCaret.bottom > gti.rcCaret.top)) {
                RECT r = gti.rcCaret;
                MapWindowPoints(gti.hwndCaret, NULL, (LPPOINT)&r, 2);
                out = r;
                ok = TRUE;
            } else if (gti.hwndFocus && IsWindow(gti.hwndFocus)) {
                POINT pt = {0, 0};
                if (GetCaretPos(&pt) && (pt.x != 0 || pt.y != 0)) {
                    ClientToScreen(gti.hwndFocus, &pt);
                    out.left = pt.x;
                    out.top = pt.y;
                    out.right = pt.x + 2;
                    out.bottom = pt.y + 20;
                    ok = TRUE;
                }
            }
        }
    }

    /* Fallback 2: Cached caret from active composition session */
    if (!ok) {
        EnterCriticalSection(&g_suggest.cs);
        if (g_suggest.last_caret_valid) {
            out = g_suggest.last_caret;
            ok = TRUE;
        }
        LeaveCriticalSection(&g_suggest.cs);
    }

    /* Fallback 3: Active/Focused window or cursor position */
    if (!ok) {
        HWND fg = GetForegroundWindow();
        if (fg && IsWindow(fg)) {
            RECT rWnd;
            if (GetWindowRect(fg, &rWnd)) {
                POINT cur;
                if (GetCursorPos(&cur) && PtInRect(&rWnd, cur)) {
                    out.left = cur.x;
                    out.top = cur.y;
                    out.right = cur.x + 2;
                    out.bottom = cur.y + 20;
                    ok = TRUE;
                } else {
                    out.left = rWnd.left + 40;
                    out.top = rWnd.bottom - 60;
                    out.right = out.left + 2;
                    out.bottom = out.top + 20;
                    ok = TRUE;
                }
            }
        }
    }

    if (ok) {
        EnterCriticalSection(&g_suggest.cs);
        g_suggest.last_caret = out;
        g_suggest.last_caret_valid = TRUE;
        LeaveCriticalSection(&g_suggest.cs);
        *rc = out;
        return TRUE;
    }
    return FALSE;
}

/* -- tray broker ------------------------------------------------------- */

static void broker_send(const GtvSuggestIpcPayload *payload) {
    HWND tray = FindWindowA(GTV_TRAY_WINDOW_CLASS, NULL);
    if (!tray) return;
    COPYDATASTRUCT cds;
    cds.dwData = (ULONG_PTR)GTV_SUGGEST_COPYDATA_ID;
    cds.cbData = (DWORD)sizeof(*payload);
    cds.lpData = (PVOID)payload;
    SendMessageTimeoutA(tray, WM_COPYDATA, 0, (LPARAM)&cds,
                        SMTO_ABORTIFHUNG, 500, NULL);
}

static void broker_send_hide(guint generation) {
    GtvSuggestIpcPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.version = GTV_SUGGEST_IPC_VERSION;
    payload.command = GTV_SUGGEST_IPC_HIDE;
    payload.source_pid = GetCurrentProcessId();
    payload.generation = generation;
    payload.candidate_count = 0;
    payload.selected = 0;
    broker_send(&payload);
}

static void broker_send_show(guint generation, gchar **candidates, guint count,
                             gint selected, RECT caret) {
    GtvSuggestIpcPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.version = GTV_SUGGEST_IPC_VERSION;
    payload.command = GTV_SUGGEST_IPC_SHOW;
    payload.source_pid = GetCurrentProcessId();
    payload.generation = generation;
    payload.caret_screen = caret;
    guint n = count > GTV_SUGGEST_IPC_MAX_CANDIDATES ? GTV_SUGGEST_IPC_MAX_CANDIDATES : count;
    for (guint i = 0; i < n; i++)
        g_strlcpy(payload.candidates[i], candidates[i] ? candidates[i] : "",
                  GTV_SUGGEST_IPC_CANDIDATE_BYTES);
    payload.candidate_count = n;
    payload.selected = (selected >= 0 && (guint)selected < n) ? (DWORD)selected : 0;
    broker_send(&payload);
}

/* -- worker ------------------------------------------------------------ */

typedef struct {
    gchar *query;
    gboolean bad;
    guint generation;
    GCancellable *cancel;
} SuggestJob;

static void suggest_job_free(gpointer data) {
    SuggestJob *job = (SuggestJob *)data;
    if (!job) return;
    g_free(job->query);
    if (job->cancel) g_object_unref(job->cancel);
    g_free(job);
}

static gpointer suggest_worker(gpointer data) {
    SuggestJob *job = (SuggestJob *)data;

    GtvConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gtv_config_load(&cfg, dir);
    g_free(dir);

    /* Debounce on the worker: AI calls wait 350ms, local offline completions only 50ms */
    int debounce = cfg.ai_enabled ? GTV_SUGGEST_DEBOUNCE_MS : 50;
    for (int waited = 0; waited < debounce; waited += 25) {
        g_usleep(25 * 1000);
        if (g_cancellable_is_cancelled(job->cancel)) {
            gtv_config_clear(&cfg);
            suggest_job_free(job);
            return NULL;
        }
    }

    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    gboolean used_ai = FALSE;
    if (cfg.ai_enabled && !g_cancellable_is_cancelled(job->cancel)) {
        GPtrArray *ai = gtv_suggest_combined(&cfg, "", job->query, job->bad);
        if (g_cancellable_is_cancelled(job->cancel)) {
            if (ai) g_ptr_array_unref(ai);
        } else if (ai) {
            for (guint i = 0; i < ai->len && out->len < GTV_SUGGEST_MAX; i++)
                g_ptr_array_add(out, g_strdup((gchar *)ai->pdata[i]));
            g_ptr_array_unref(ai);
            used_ai = out->len > 0;
        }
    }
    /* Offline fallback (mirror IBus): remembered correction in typo mode,
     * learned completions otherwise. */
    if (out->len == 0 && !g_cancellable_is_cancelled(job->cancel)) {
        EnterCriticalSection(&g_suggest.cs);
        learned_ensure_locked();
        if (job->bad) {
            gchar *fix = gtv_fixes_lookup(g_suggest.learned_fixes, job->query);
            if (fix) g_ptr_array_add(out, fix);
        } else {
            gtv_learned_completions(g_suggest.learned_words, job->query, out,
                                    GTV_SUGGEST_MAX);
        }
        LeaveCriticalSection(&g_suggest.cs);
    }
    gtv_config_clear(&cfg);

    /* Publish under the lock, send outside it (the tray round-trip can block). */
    gchar *snapshot[GTV_SUGGEST_MAX];
    guint n = 0;
    RECT caret;
    gboolean have_caret = FALSE;
    gint cursor = 0;
    guint cur_gen = 0;
    gboolean stale = TRUE;
    memset(&caret, 0, sizeof(caret));
    EnterCriticalSection(&g_suggest.cs);
    stale = g_cancellable_is_cancelled(job->cancel) || job->generation != g_suggest.generation;
    cur_gen = g_suggest.generation;
    if (!stale) {
        free_items(g_suggest.shown);
        if (out->len > 0) {
            for (guint i = 0; i < out->len && i < GTV_SUGGEST_MAX; i++) {
                SuggestItem *it = g_new0(SuggestItem, 1);
                it->text = g_strdup((gchar *)out->pdata[i]);
                it->from_ai = used_ai;
                g_ptr_array_add(g_suggest.shown, it);
            }
            g_suggest.cursor = 0;
        }
        cursor = g_suggest.cursor;
        caret = g_suggest.caret;
        have_caret = g_suggest.caret_valid;
        n = g_suggest.shown->len > GTV_SUGGEST_MAX ? GTV_SUGGEST_MAX
                                                  : (guint)g_suggest.shown->len;
        for (guint i = 0; i < n; i++) {
            SuggestItem *it = (SuggestItem *)g_ptr_array_index(g_suggest.shown, i);
            snapshot[i] = g_strdup(it->text ? it->text : "");
        }
    }
    LeaveCriticalSection(&g_suggest.cs);
    g_ptr_array_unref(out);
    if (stale) {
        suggest_job_free(job);
        return NULL;
    }
    if (n == 0 || !have_caret) {
        /* Nothing to show (or nowhere to show it): hide explicitly so a
         * previous query's overlay cannot linger. Same generation, so this
         * always wins over older shows. */
        broker_send_hide(cur_gen);
    } else {
        broker_send_show(cur_gen, snapshot, n, cursor, caret);
    }
    for (guint i = 0; i < n; i++)
        g_free(snapshot[i]);
    suggest_job_free(job);
    return NULL;
}

/* -- commit helpers ----------------------------------------------------- */

static void engine_set_raw(GtvEngine *eng, const gchar *utf8) {
    gtv_engine_reset(eng);
    if (!utf8) return;
    for (const gchar *p = utf8; *p; p = g_utf8_next_char(p)) {
        gunichar ch = g_utf8_get_char(p);
        g_array_append_val(eng->buffer, ch);
    }
}

static HRESULT commit_candidate(CGtvTextService *service, ITfContext *pic,
                                const gchar *text, gboolean keep_composing) {
    if (!service || !pic || !text) return E_INVALIDARG;
    GtvEngine *eng = service->GetEngine();
    if (!eng) return E_FAIL;
    HRESULT hr;
    if (keep_composing) {
        engine_set_raw(eng, text);
        gchar *buf = gtv_engine_buffer(eng);
        if (buf && *buf) {
            glong wlen = 0;
            wchar_t *w = (wchar_t *)g_utf8_to_utf16(buf, -1, NULL, &wlen, NULL);
            if (w) {
                hr = service->UpdateCompositionText(pic, w, (int)wlen);
                g_free(w);
            } else {
                hr = E_FAIL;
            }
        } else {
            hr = service->EndComposition(pic, FALSE);
        }
        g_free(buf);
    } else {
        glong wlen = 0;
        wchar_t *w = (wchar_t *)g_utf8_to_utf16(text, -1, NULL, &wlen, NULL);
        if (!w) return E_FAIL;
        hr = service->UpdateCompositionText(pic, w, (int)wlen);
        g_free(w);
        if (SUCCEEDED(hr)) hr = service->EndComposition(pic, TRUE);
        gtv_engine_reset(eng);
    }
    return hr;
}

/* -- public API ---------------------------------------------------------- */

/* New generation + cancel in-flight work without notifying the tray (used
 * per keystroke to avoid flicker; the next show/hide supersedes). */
static guint suggest_restart_locked(void) {
    g_suggest.generation++;
    if (g_suggest.cancellable) {
        g_cancellable_cancel(g_suggest.cancellable);
        g_object_unref(g_suggest.cancellable);
        g_suggest.cancellable = NULL;
    }
    return g_suggest.generation;
}

void gtv_suggest_hide(void) {
    suggest_init_once();
    guint gen = 0;
    EnterCriticalSection(&g_suggest.cs);
    gen = suggest_restart_locked();
    free_items(g_suggest.shown);
    g_suggest.cursor = 0;
    g_suggest.caret_valid = FALSE;
    g_suggest.last_caret_valid = FALSE;
    g_free(g_suggest.pending_query);
    g_suggest.pending_query = NULL;
    LeaveCriticalSection(&g_suggest.cs);
    broker_send_hide(gen);
}

gboolean gtv_suggest_visible(void) {
    suggest_init_once();
    EnterCriticalSection(&g_suggest.cs);
    gboolean vis = g_suggest.shown && g_suggest.shown->len > 0;
    LeaveCriticalSection(&g_suggest.cs);
    return vis;
}

gint gtv_suggest_count(void) {
    suggest_init_once();
    EnterCriticalSection(&g_suggest.cs);
    gint n = (g_suggest.shown && g_suggest.shown->len > 0) ? (gint)g_suggest.shown->len : 0;
    LeaveCriticalSection(&g_suggest.cs);
    return n;
}

void gtv_suggest_move_cursor(gint delta) {
    suggest_init_once();
    gchar *snapshot[GTV_SUGGEST_MAX];
    guint n = 0;
    RECT caret;
    gboolean have_caret = FALSE;
    gint cursor = 0;
    guint gen = 0;
    memset(&caret, 0, sizeof(caret));
    EnterCriticalSection(&g_suggest.cs);
    if (g_suggest.shown && g_suggest.shown->len > 0) {
        gint count = (gint)g_suggest.shown->len;
        g_suggest.cursor = (g_suggest.cursor + delta % count + count) % count;
        cursor = g_suggest.cursor;
        caret = g_suggest.caret;
        have_caret = g_suggest.caret_valid;
        gen = g_suggest.generation;
        n = (guint)count > GTV_SUGGEST_MAX ? GTV_SUGGEST_MAX : (guint)count;
        for (guint i = 0; i < n; i++) {
            SuggestItem *it = (SuggestItem *)g_ptr_array_index(g_suggest.shown, i);
            snapshot[i] = g_strdup(it->text ? it->text : "");
        }
    }
    LeaveCriticalSection(&g_suggest.cs);
    if (n > 0 && have_caret)
        broker_send_show(gen, snapshot, n, cursor, caret);
    for (guint i = 0; i < n; i++)
        g_free(snapshot[i]);
}

static void accept_locked(CGtvTextService *service, ITfContext *pic,
                          const gchar *text, gboolean from_ai,
                          const gchar *typed, gboolean keep_composing) {
    if (!text || !*text) return;
    commit_candidate(service, pic, text, keep_composing);
    if (from_ai && typed && strcmp(typed, text) != 0) {
        EnterCriticalSection(&g_suggest.cs);
        learn_accepted_locked(typed, text);
        LeaveCriticalSection(&g_suggest.cs);
    }
    if (typed) notify_tray_word(text);
    gtv_suggest_hide();
}

void gtv_suggest_accept_cursor(CGtvTextService *service, ITfContext *pic) {
    if (!service) return;
    gchar *text = NULL;
    gboolean from_ai = FALSE;
    gchar *typed = NULL;
    EnterCriticalSection(&g_suggest.cs);
    if (g_suggest.shown && g_suggest.cursor >= 0 &&
        g_suggest.cursor < (gint)g_suggest.shown->len) {
        SuggestItem *it = (SuggestItem *)g_ptr_array_index(g_suggest.shown, g_suggest.cursor);
        text = g_strdup(it->text);
        from_ai = it->from_ai;
        typed = g_strdup(g_suggest.pending_query);
    }
    LeaveCriticalSection(&g_suggest.cs);
    if (text) accept_locked(service, pic, text, from_ai, typed, TRUE);
    g_free(text);
    g_free(typed);
}

void gtv_suggest_commit_cursor(CGtvTextService *service, ITfContext *pic) {
    if (!service) return;
    gchar *text = NULL;
    gboolean from_ai = FALSE;
    gchar *typed = NULL;
    EnterCriticalSection(&g_suggest.cs);
    if (g_suggest.shown && g_suggest.cursor >= 0 &&
        g_suggest.cursor < (gint)g_suggest.shown->len) {
        SuggestItem *it = (SuggestItem *)g_ptr_array_index(g_suggest.shown, g_suggest.cursor);
        text = g_strdup(it->text);
        from_ai = it->from_ai;
        typed = g_strdup(g_suggest.pending_query);
    }
    LeaveCriticalSection(&g_suggest.cs);
    if (text) accept_locked(service, pic, text, from_ai, typed, FALSE);
    g_free(text);
    g_free(typed);
}

void gtv_suggest_accept_index(CGtvTextService *service, ITfContext *pic, gint index) {
    if (!service) return;
    gchar *text = NULL;
    gboolean from_ai = FALSE;
    gchar *typed = NULL;
    EnterCriticalSection(&g_suggest.cs);
    if (g_suggest.shown && index >= 0 && index < (gint)g_suggest.shown->len) {
        SuggestItem *it = (SuggestItem *)g_ptr_array_index(g_suggest.shown, index);
        text = g_strdup(it->text);
        from_ai = it->from_ai;
        typed = g_strdup(g_suggest.pending_query);
    }
    LeaveCriticalSection(&g_suggest.cs);
    if (text) accept_locked(service, pic, text, from_ai, typed, FALSE);
    g_free(text);
    g_free(typed);
}

gboolean gtv_suggest_has_exact_match(CGtvTextService *service) {
    if (!service || !service->IsComposing()) return FALSE;
    GtvEngine *eng = service->GetEngine();
    if (!eng) return FALSE;
    gchar *buf = gtv_engine_buffer(eng);
    if (!buf || !*buf) {
        g_free(buf);
        return FALSE;
    }
    gchar *hit = expand_macro(buf);
    if (!hit) hit = expand_emoji(buf);
    g_free(buf);
    if (hit) {
        g_free(hit);
        return TRUE;
    }
    return FALSE;
}

void gtv_suggest_on_key(CGtvTextService *service, ITfContext *pic) {
    suggest_init_once();
    /* Cancel anything in flight first (new keystroke restarts the story),
     * then take a fresh generation so stale workers cannot publish. No
     * HIDE is sent here: the next show/hide supersedes, avoiding flicker. */
    EnterCriticalSection(&g_suggest.cs);
    guint gen = suggest_restart_locked();
    LeaveCriticalSection(&g_suggest.cs);
    if (!service || !pic || !service->IsComposing()) {
        gtv_suggest_hide();
        return;
    }
    GtvEngine *eng = service->GetEngine();
    if (!eng) {
        gtv_suggest_hide();
        return;
    }
    gchar *buf = gtv_engine_buffer(eng);
    if (!buf || !*buf) {
        g_free(buf);
        gtv_suggest_hide();
        return;
    }
    glong len = g_utf8_strlen(buf, -1);

    /* Caret is captured on the app thread now; the worker only forwards it.
     * No rect -> no overlay (never fall back to a screen corner). */
    RECT caret;
    gboolean have_caret = (gboolean)service->GetCaretRect(pic, &caret);

    gunichar first = g_utf8_get_char(buf);
    if ((first == ':' || first == ';' || first == '<' || first == '(') && len >= 2) {
        /* Emoji prefix: instant local suggestions, no AI needed. */
        GPtrArray *sugs = get_emoji_suggestions(buf);
        if (sugs && sugs->len > 0 && have_caret) {
            gchar *snapshot[GTV_SUGGEST_MAX];
            guint n = 0;
            EnterCriticalSection(&g_suggest.cs);
            free_items(g_suggest.shown);
            for (guint i = 0; i < sugs->len && i < GTV_SUGGEST_MAX; i++) {
                SuggestItem *it = g_new0(SuggestItem, 1);
                it->text = g_strdup((gchar *)sugs->pdata[i]);
                it->from_ai = FALSE;
                g_ptr_array_add(g_suggest.shown, it);
            }
            g_suggest.cursor = 0;
            g_free(g_suggest.pending_query);
            g_suggest.pending_query = g_strdup(buf);
            g_suggest.pending_bad = FALSE;
            g_suggest.caret = caret;
            g_suggest.caret_valid = TRUE;
            n = g_suggest.shown->len > GTV_SUGGEST_MAX ? GTV_SUGGEST_MAX
                                                      : (guint)g_suggest.shown->len;
            for (guint i = 0; i < n; i++) {
                SuggestItem *it = (SuggestItem *)g_ptr_array_index(g_suggest.shown, i);
                snapshot[i] = g_strdup(it->text ? it->text : "");
            }
            LeaveCriticalSection(&g_suggest.cs);
            broker_send_show(gen, snapshot, n, 0, caret);
            for (guint i = 0; i < n; i++)
                g_free(snapshot[i]);
        } else {
            gtv_suggest_hide();
        }
        if (sugs) g_ptr_array_unref(sugs);
        g_free(buf);
        return;
    }

    /* Suggestions path: check if either AI suggestions or spellcheck/smart completions are enabled */
    {
        GtvConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
        gtv_config_load(&cfg, dir);
        g_free(dir);
        gboolean enabled = cfg.ai_enabled || cfg.spellcheck;
        gtv_config_clear(&cfg);
        if (!enabled) {
            g_free(buf);
            gtv_suggest_hide();
            return;
        }
    }
    {
        gboolean bad = eng->spellcheck && !suggest_word_known(buf);
        GCancellable *cancel = NULL;
        EnterCriticalSection(&g_suggest.cs);
        g_free(g_suggest.pending_query);
        g_suggest.pending_query = g_strdup(buf);
        g_suggest.pending_bad = bad;
        g_suggest.caret = caret;
        g_suggest.caret_valid = have_caret;
        g_suggest.cancellable = g_cancellable_new();
        if (g_suggest.cancellable)
            cancel = (GCancellable *)g_object_ref(g_suggest.cancellable);
        LeaveCriticalSection(&g_suggest.cs);
        g_free(buf);
        if (!cancel) return;
        SuggestJob *job = g_new0(SuggestJob, 1);
        job->query = g_strdup(g_suggest.pending_query);
        job->bad = bad;
        job->generation = gen;
        job->cancel = cancel;
        GThread *th = g_thread_new("gtv-suggest", suggest_worker, job);
        if (!th) {
            suggest_job_free(job);
        } else {
            g_thread_unref(th);
        }
    }
}
