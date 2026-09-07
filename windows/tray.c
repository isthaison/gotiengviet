#include "tray.h"
#include "hook.h"
#include "setup.h"
#include "resource.h"
#include "internal.h"

static NOTIFYICONDATA nid = {0};
static HICON icon_v = NULL;
static HICON icon_e = NULL;

static gboolean get_startup_enabled(void) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return FALSE;
    char path[MAX_PATH];
    DWORD size = sizeof(path);
    LONG res = RegQueryValueEx(hkey, "GoTiengViet", NULL, NULL, (LPBYTE)path, &size);
    RegCloseKey(hkey);
    return res == ERROR_SUCCESS;
}

/* Guards g_app.config string fields: the setup dialog swaps them on the UI
 * thread while the AI worker may be copying them. First use is always on
 * the UI thread (hook callbacks run there too). */
static CRITICAL_SECTION config_lock;
static gboolean config_lock_ready = FALSE;
void gtv_config_strings_lock(void) {
    if (!config_lock_ready) {
        InitializeCriticalSection(&config_lock);
        config_lock_ready = TRUE;
    }
    EnterCriticalSection(&config_lock);
}
void gtv_config_strings_unlock(void) {
    LeaveCriticalSection(&config_lock);
}

/* Config lives in %APPDATA%/gotiengviet (same dir main.c loads from). */
static void save_app_config(void) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gtv_config_save(&g_app.config, dir, NULL);
    g_free(dir);
}

void gtv_tray_set_startup(gboolean enable) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
        return;
    if (enable) {
        char path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        RegSetValueEx(hkey, "GoTiengViet", 0, REG_SZ, (const BYTE *)path, (DWORD)strlen(path) + 1);
    } else {
        RegDeleteValue(hkey, "GoTiengViet");
    }
    RegCloseKey(hkey);
}

gboolean gtv_tray_init(HWND hwnd) {
    InitializeCriticalSection(&learned_lock);
    HINSTANCE hinst = GetModuleHandle(NULL);
    icon_v = LoadIcon(hinst, MAKEINTRESOURCE(IDI_TRAY_V));
    icon_e = LoadIcon(hinst, MAKEINTRESOURCE(IDI_TRAY_E));

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_CALLBACK;
    nid.hIcon = g_app.enabled ? icon_v : icon_e;
    strcpy(nid.szTip, "GoTiengViet - Bộ gõ tiếng Việt");

    return Shell_NotifyIcon(NIM_ADD, &nid);
}

void gtv_tray_cleanup(void) {
    DeleteCriticalSection(&learned_lock);
    if (config_lock_ready) {
        DeleteCriticalSection(&config_lock);
        config_lock_ready = FALSE;
    }
    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (icon_v) DestroyIcon(icon_v);
    if (icon_e) DestroyIcon(icon_e);
}

void gtv_tray_update_icon(gboolean enabled) {
    nid.uFlags &= (UINT)~NIF_INFO; /* drop stale balloon text on icon updates */
    nid.hIcon = enabled ? icon_v : icon_e;
    if (enabled) {
        strcpy(nid.szTip, "GoTiengViet [Tiếng Việt]");
    } else {
        strcpy(nid.szTip, "GoTiengViet [English]");
    }
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void gtv_tray_show_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hmenu = CreatePopupMenu();
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, "Bảng điều khiển...");
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    UINT telex_flag = (g_app.config.mode == GTV_TELEX) ? MF_CHECKED : MF_UNCHECKED;
    UINT vni_flag = (g_app.config.mode == GTV_VNI) ? MF_CHECKED : MF_UNCHECKED;
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | telex_flag, ID_TRAY_MODE_TELEX, "Kiểu gõ Telex");
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | vni_flag, ID_TRAY_MODE_VNI, "Kiểu gõ VNI");

    UINT spell_flag = g_app.config.spellcheck ? MF_CHECKED : MF_UNCHECKED;
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | spell_flag, ID_TRAY_SPELLCHECK, "Kiểm tra chính tả");

    UINT modern_flag = g_app.config.modern ? MF_CHECKED : MF_UNCHECKED;
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | modern_flag, ID_TRAY_MODERN, "Đặt dấu chuẩn mới");

    UINT start_flag = get_startup_enabled() ? MF_CHECKED : MF_UNCHECKED;
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | start_flag, ID_TRAY_STARTUP, "Khởi động cùng Windows");

    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, "Thoát");

    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hmenu);

    switch (cmd) {
        case ID_TRAY_SETTINGS:
            gtv_setup_show(hwnd);
            break;
        case ID_TRAY_MODE_TELEX:
            g_app.config.mode = GTV_TELEX;
            if (g_app.engine) g_app.engine->mode = GTV_TELEX;
            save_app_config();
            break;
        case ID_TRAY_MODE_VNI:
            g_app.config.mode = GTV_VNI;
            if (g_app.engine) g_app.engine->mode = GTV_VNI;
            save_app_config();
            break;
        case ID_TRAY_SPELLCHECK:
            g_app.config.spellcheck = !g_app.config.spellcheck;
            if (g_app.engine) g_app.engine->spellcheck = g_app.config.spellcheck;
            save_app_config();
            break;
        case ID_TRAY_MODERN:
            g_app.config.modern = !g_app.config.modern;
            if (g_app.engine) g_app.engine->modern = g_app.config.modern;
            save_app_config();
            break;
        case ID_TRAY_STARTUP:
            gtv_tray_set_startup(!get_startup_enabled());
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
    }
}

/* Tray balloon, throttled so a burst of typos does not spam. The tray API
 * here is ANSI, so UTF-8 text is converted to Windows-1258 (raw UTF-8 bytes
 * would render as mojibake); conversion failure falls back to raw text. */
void gtv_tray_balloon(const gchar *title, const gchar *msg) {
    static DWORD last_tick = 0;
    DWORD now = GetTickCount();
    if (last_tick && (now - last_tick) < 10000) return;
    last_tick = now;
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    gchar *ansi_title = title ? g_convert(title, -1, "WINDOWS-1258", "UTF-8", NULL, NULL, NULL) : NULL;
    gchar *ansi_msg = msg ? g_convert(msg, -1, "WINDOWS-1258", "UTF-8", NULL, NULL, NULL) : NULL;
    g_strlcpy(nid.szInfoTitle, ansi_title ? ansi_title : (title ? title : "GoTiengViet"), sizeof(nid.szInfoTitle));
    g_strlcpy(nid.szInfo, ansi_msg ? ansi_msg : (msg ? msg : ""), sizeof(nid.szInfo));
    g_free(ansi_title);
    g_free(ansi_msg);
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

typedef struct { gchar *word; gchar *model; gchar *url; } AiJob;

static volatile LONG ai_in_flight = 0;

/* Learned store shared with the AI worker thread (CRITICAL_SECTION guards
 * it; initialized in gtv_tray_init before any worker can exist). */
static GHashTable *w_learned_words = NULL;
static GHashTable *w_learned_fixes = NULL;
static CRITICAL_SECTION learned_lock;
static void w_learned_ensure(void){
    if(!w_learned_words){
        w_learned_words = gtv_words_table_new();
        gchar *p = gtv_learned_path("learned-words.txt");
        gtv_words_load(w_learned_words, p);
        g_free(p);
    }
    if(!w_learned_fixes){
        w_learned_fixes = gtv_fixes_table_new();
        gchar *p = gtv_learned_path("learned-corrections.txt");
        gtv_fixes_load(w_learned_fixes, p);
        g_free(p);
    }
}

/* Pending suggestion behind the balloon: click applies it when nothing was
 * typed since, otherwise the correction is copied to the clipboard. */
static gchar *pending_typed = NULL;
static gchar *pending_fix = NULL;
static DWORD pending_tick = 0;
static DWORD pending_input_tick = 0;

void gtv_tray_suggest_balloon(const gchar *typed, const gchar *correction){
    g_free(pending_typed); g_free(pending_fix);
    pending_typed = pending_fix = NULL;
    if(!typed || !*typed || !correction || !*correction) return;
    pending_typed = g_strdup(typed);
    pending_fix = g_strdup(correction);
    pending_tick = GetTickCount();
    pending_input_tick = g_app.last_input_tick;
    gchar *msg = g_strdup_printf("\"%s\" co the ban muon go \"%s\"?", typed, correction);
    gtv_tray_balloon("GoTiengViet goi y", msg);
    g_free(msg);
}

static void copy_to_clipboard(const gchar *utf8){
    if(!utf8 || !g_app.hwnd_main) return;
    glong wlen = 0;
    gunichar2 *wstr = g_utf8_to_utf16(utf8, -1, NULL, &wlen, NULL);
    if(!wstr) return;
    if(OpenClipboard(g_app.hwnd_main)){
        EmptyClipboard();
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(gunichar2));
        if(h){
            gunichar2 *dst = GlobalLock(h);
            if(dst){
                memcpy(dst, wstr, (wlen + 1) * sizeof(gunichar2));
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);
            }else GlobalFree(h);
        }
        CloseClipboard();
    }
    g_free(wstr);
}

static void learn_accepted(const gchar *typed, const gchar *fix){
    EnterCriticalSection(&learned_lock);
    w_learned_ensure();
    gboolean dirty_fix = gtv_fixes_learn(w_learned_fixes, typed, fix);
    gboolean dirty_words = FALSE;
    gchar **words = g_strsplit_set(fix, " \t", -1);
    for(guint i = 0; words[i]; i++)
        if(gtv_words_learn(w_learned_words, words[i])) dirty_words = TRUE;
    g_strfreev(words);
    if(dirty_fix){
        gchar *p = gtv_learned_path("learned-corrections.txt");
        gtv_fixes_save(w_learned_fixes, p);
        g_free(p);
    }
    if(dirty_words){
        gchar *p = gtv_learned_path("learned-words.txt");
        gtv_words_save(w_learned_words, p);
        g_free(p);
    }
    LeaveCriticalSection(&learned_lock);
}

void gtv_tray_apply_pending(void){
    if(!pending_typed || !pending_fix) return;
    gchar *typed = pending_typed, *fix = pending_fix;
    pending_typed = pending_fix = NULL;
    gboolean fresh = (DWORD)(GetTickCount() - pending_tick) < 15000
        && g_app.last_input_tick == pending_input_tick;
    if(fresh && g_utf8_validate(typed, -1, NULL) && g_utf8_validate(fix, -1, NULL)
       && g_utf8_strlen(typed, -1) >= 2 && g_utf8_strlen(typed, -1) <= 64){
        gtv_hook_send_backspaces((int)g_utf8_strlen(typed, -1));
        gtv_hook_send_text(fix);
    }else{
        copy_to_clipboard(fix);
    }
    learn_accepted(typed, fix);
    g_free(typed); g_free(fix);
}

static gpointer ai_worker(gpointer data) {
    AiJob *job = data;
    GtvConfig cfg = {0};
    cfg.ai_enabled = TRUE;
    cfg.model = job->model;
    cfg.url = job->url;
    GPtrArray *sugs = gtv_suggest_combined(&cfg, "", job->word, TRUE);
    gchar *fix = NULL;
    if (sugs && sugs->len > 0 && g_strcmp0(sugs->pdata[0], job->word) != 0)
        fix = g_strdup(sugs->pdata[0]);
    if (sugs) g_ptr_array_unref(sugs);
    if (!fix) {
        /* Ollama unreachable: reuse Ollama-taught corrections offline. */
        EnterCriticalSection(&learned_lock);
        w_learned_ensure();
        fix = gtv_fixes_lookup(w_learned_fixes, job->word);
        LeaveCriticalSection(&learned_lock);
        if (fix && !g_strcmp0(fix, job->word)) { g_free(fix); fix = NULL; }
    }
    if (fix && g_app.hwnd_main) {
        GtvAiResult *res = g_new(GtvAiResult, 1);
        res->typed = job->word; job->word = NULL;
        res->fix = fix; fix = NULL;
        PostMessage(g_app.hwnd_main, WM_GTV_AI_RESULT, 0, (LPARAM)res);
    }
    g_free(fix);
    g_free(job->word); g_free(job->model); g_free(job->url); g_free(job);
    InterlockedExchange(&ai_in_flight, 0);
    return NULL;
}

void gtv_tray_check_spelling_async(const gchar *word) {
    if (!word || InterlockedCompareExchange(&ai_in_flight, 1, 0) != 0) return;
    AiJob *job = g_new0(AiJob, 1);
    job->word = g_strdup(word);
    gtv_config_strings_lock();
    job->model = g_strdup(g_app.config.model);
    job->url = g_strdup(g_app.config.url);
    gtv_config_strings_unlock();
    if (!job->model || !job->url) {
        g_free(job->word); g_free(job->model); g_free(job->url); g_free(job);
        InterlockedExchange(&ai_in_flight, 0);
        return;
    }
    GThread *th = g_thread_new("gtv-ai-check", ai_worker, job);
    if (!th) {
        g_free(job->word); g_free(job->model); g_free(job->url); g_free(job);
        InterlockedExchange(&ai_in_flight, 0);
        return;
    }
    g_thread_unref(th);
}
