#include "tray.h"
#include "app.h"
#include "setup.h"
#include "update.h"
#include "resource.h"
#include "internal.h"

static NOTIFYICONDATAW nid = {0};
static HICON icon_v = NULL;
static HICON icon_e = NULL;

/* All UI strings in this codebase are UTF-8; the tray/menu/registry APIs
 * below are the explicit W variants so Vietnamese renders correctly on
 * any system locale (ANSI A-APIs would decode UTF-8 as mojibake). */
static void wstr_copy(WCHAR *dst, guint dst_chars, const gunichar2 *src) {
    guint i = 0;
    if (src) while (i + 1 < dst_chars && src[i]) { dst[i] = (WCHAR)src[i]; i++; }
    /* Never split a surrogate pair at the truncation edge. */
    if (i > 0 && i + 1 >= dst_chars && dst[i - 1] >= 0xD800 && dst[i - 1] <= 0xDBFF) i--;
    dst[i] = 0;
}
static void set_field(const gchar *utf8, WCHAR *dst, guint dst_chars) {
    gunichar2 *w = utf8 ? g_utf8_to_utf16(utf8, -1, NULL, NULL, NULL) : NULL;
    wstr_copy(dst, dst_chars, w);
    g_free(w);
}
static void menu_add(HMENU hmenu, const gchar *utf8, UINT flags, UINT_PTR id) {
    if (flags & MF_SEPARATOR) {
        InsertMenuW(hmenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        return;
    }
    gunichar2 *w = g_utf8_to_utf16(utf8 ? utf8 : "", -1, NULL, NULL, NULL);
    InsertMenuW(hmenu, -1, MF_BYPOSITION | MF_STRING | flags, id, (LPCWSTR)w);
    g_free(w);
}

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
/* Learned store shared with the AI worker thread (CRITICAL_SECTION guards
 * it; initialized in gtv_tray_init before any worker can exist). Declared
 * up here because gtv_tray_init/cleanup run before its definition site. */
static CRITICAL_SECTION learned_lock;
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


void gtv_tray_set_startup(gboolean enable) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
        return;
    if (enable) {
        WCHAR path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        RegSetValueExW(hkey, L"GoTiengViet", 0, REG_SZ, (const BYTE *)path,
                       (DWORD)(lstrlenW(path) + 1) * sizeof(WCHAR));
    } else {
        RegDeleteValueW(hkey, L"GoTiengViet");
    }
    RegCloseKey(hkey);
}

gboolean gtv_tray_init(HWND hwnd) {
    InitializeCriticalSection(&learned_lock);
    HINSTANCE hinst = GetModuleHandle(NULL);
    icon_v = LoadIcon(hinst, MAKEINTRESOURCE(IDI_TRAY_V));
    icon_e = LoadIcon(hinst, MAKEINTRESOURCE(IDI_TRAY_E));

    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_CALLBACK;
    nid.hIcon = g_app.enabled ? icon_v : icon_e;
    set_field("GoTiengViet - Bộ gõ tiếng Việt", nid.szTip, G_N_ELEMENTS(nid.szTip));

    return Shell_NotifyIconW(NIM_ADD, &nid);
}

void gtv_tray_cleanup(void) {
    DeleteCriticalSection(&learned_lock);
    if (config_lock_ready) {
        DeleteCriticalSection(&config_lock);
        config_lock_ready = FALSE;
    }
    Shell_NotifyIconW(NIM_DELETE, &nid);
    if (icon_v) DestroyIcon(icon_v);
    if (icon_e) DestroyIcon(icon_e);
}

void gtv_tray_update_icon(gboolean enabled) {
    nid.uFlags &= (UINT)~NIF_INFO; /* drop stale balloon text on icon updates */
    nid.hIcon = enabled ? icon_v : icon_e;
    if (enabled) {
        set_field("GoTiengViet [Tiếng Việt]", nid.szTip, G_N_ELEMENTS(nid.szTip));
    } else {
        set_field("GoTiengViet [English]", nid.szTip, G_N_ELEMENTS(nid.szTip));
    }
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void gtv_tray_show_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hmenu = CreatePopupMenu();
    UINT toggle_flag = g_app.enabled ? MF_CHECKED : MF_UNCHECKED;
    menu_add(hmenu, "Bật gõ tiếng Việt [V]", toggle_flag, ID_TRAY_TOGGLE);
    menu_add(hmenu, "Bảng điều khiển...", 0, ID_TRAY_SETTINGS);
    menu_add(hmenu, NULL, MF_SEPARATOR, 0);

    UINT telex_flag = (g_app.config.mode == GTV_TELEX) ? MF_CHECKED : MF_UNCHECKED;
    UINT vni_flag = (g_app.config.mode == GTV_VNI) ? MF_CHECKED : MF_UNCHECKED;
    menu_add(hmenu, "Kiểu gõ Telex", telex_flag, ID_TRAY_MODE_TELEX);
    menu_add(hmenu, "Kiểu gõ VNI", vni_flag, ID_TRAY_MODE_VNI);

    UINT spell_flag = g_app.config.spellcheck ? MF_CHECKED : MF_UNCHECKED;
    menu_add(hmenu, "Kiểm tra chính tả", spell_flag, ID_TRAY_SPELLCHECK);

    UINT modern_flag = g_app.config.modern ? MF_CHECKED : MF_UNCHECKED;
    menu_add(hmenu, "Đặt dấu chuẩn mới", modern_flag, ID_TRAY_MODERN);

    UINT start_flag = get_startup_enabled() ? MF_CHECKED : MF_UNCHECKED;
    menu_add(hmenu, "Khởi động cùng Windows", start_flag, ID_TRAY_STARTUP);

    menu_add(hmenu, "Kiểm tra cập nhật...", 0, ID_TRAY_UPDATE);
    menu_add(hmenu, NULL, MF_SEPARATOR, 0);
    menu_add(hmenu, "Thoát", 0, ID_TRAY_EXIT);

    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hmenu);

    switch (cmd) {
        case ID_TRAY_TOGGLE:
            gtv_app_toggle_mode();
            break;
        case ID_TRAY_SETTINGS:
            gtv_setup_show(hwnd);
            break;
        case ID_TRAY_MODE_TELEX:
            gtv_app_set_input_method(GTV_TELEX);
            break;
        case ID_TRAY_MODE_VNI:
            gtv_app_set_input_method(GTV_VNI);
            break;
        case ID_TRAY_SPELLCHECK:
            g_app.config.spellcheck = !g_app.config.spellcheck;
            gtv_app_save_config();
            break;
        case ID_TRAY_MODERN:
            g_app.config.modern = !g_app.config.modern;
            gtv_app_save_config();
            break;
        case ID_TRAY_STARTUP:
            gtv_tray_set_startup(!get_startup_enabled());
            break;
        case ID_TRAY_UPDATE:
            gtv_update_check_async(hwnd, TRUE);
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
    }
}

/* Tray balloon over the Unicode NOTIFYICONDATAW: UTF-8 goes straight to
 * UTF-16, correct on every system locale (the old ANSI version needed a
 * lossy Windows-1258 conversion). */
static void balloon_show(const gchar *title, const gchar *msg) {
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    set_field(title ? title : "GoTiengViet", nid.szInfoTitle, G_N_ELEMENTS(nid.szInfoTitle));
    set_field(msg ? msg : "", nid.szInfo, G_N_ELEMENTS(nid.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void gtv_tray_balloon(const gchar *title, const gchar *msg) {
    static DWORD last_tick = 0;
    DWORD now = GetTickCount();
    /* One shared slot: a burst of typos must not spam. Update flows
     * use gtv_tray_balloon_force instead so a manual check is never
     * silently swallowed right after an AI balloon. */
    if (last_tick && (now - last_tick) < 10000) return;
    last_tick = now;
    balloon_show(title, msg);
}

void gtv_tray_balloon_force(const gchar *title, const gchar *msg) {
    balloon_show(title, msg);
}

typedef struct { gchar *word; gchar *model; gchar *url; } AiJob;

static volatile LONG ai_in_flight = 0;

/* Learned store shared with the AI worker thread (lock declared above). */
static GHashTable *w_learned_words = NULL;
static GHashTable *w_learned_fixes = NULL;
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

/* Pending suggestion behind the balloon: click applies it when no word
 * was committed since (generation counter replaces the old input-tick
 * check: without a keyboard hook there are no keystroke ticks). */
static gchar *pending_typed = NULL;
static gchar *pending_fix = NULL;
static volatile LONG ai_word_generation = 0;
static LONG pending_ai_generation = 0;

void gtv_tray_suggest_balloon(const gchar *typed, const gchar *correction){
    gtv_update_disown_balloon();
    g_free(pending_typed); g_free(pending_fix);
    pending_typed = pending_fix = NULL;
    if(!typed || !*typed || !correction || !*correction) return;
    pending_typed = g_strdup(typed);
    pending_fix = g_strdup(correction);
    pending_ai_generation = ai_word_generation;
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

/* Direct keystroke injection for applying AI corrections (no low-level
 * hook exists anymore, so plain SendInput suffices). */
static void send_replace_text(glong erase_chars, const gchar *utf8) {
    glong wlen = 0;
    guint16 *wstr = NULL;
    if (utf8 && *utf8) wstr = g_utf8_to_utf16(utf8, -1, NULL, &wlen, NULL);
    if (!wstr) wlen = 0;
    if (erase_chars <= 0 && wlen == 0) { g_free(wstr); return; }
    guint total = (guint)(erase_chars * 2 + wlen * 2);
    INPUT *inputs = g_new0(INPUT, total);
    guint k = 0;
    for (glong i = 0; i < erase_chars; i++) {
        inputs[k].type = INPUT_KEYBOARD; inputs[k].ki.wVk = VK_BACK; k++;
        inputs[k].type = INPUT_KEYBOARD; inputs[k].ki.wVk = VK_BACK;
        inputs[k].ki.dwFlags = KEYEVENTF_KEYUP; k++;
    }
    for (glong i = 0; i < wlen; i++) {
        inputs[k].type = INPUT_KEYBOARD; inputs[k].ki.wScan = wstr[i];
        inputs[k].ki.dwFlags = KEYEVENTF_UNICODE; k++;
        inputs[k].type = INPUT_KEYBOARD; inputs[k].ki.wScan = wstr[i];
        inputs[k].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP; k++;
    }
    SendInput(total, inputs, sizeof(INPUT));
    g_free(inputs);
    g_free(wstr);
}

void gtv_tray_apply_pending(void){
    if(!pending_typed || !pending_fix) return;
    gchar *typed = pending_typed, *fix = pending_fix;
    pending_typed = pending_fix = NULL;
    if (ai_word_generation == pending_ai_generation
        && g_utf8_validate(typed, -1, NULL) && g_utf8_validate(fix, -1, NULL)
        && g_utf8_strlen(typed, -1) >= 2 && g_utf8_strlen(typed, -1) <= 64) {
        send_replace_text(g_utf8_strlen(typed, -1), fix);
    } else {
        copy_to_clipboard(fix);
    }
    learn_accepted(typed, fix);
    g_free(typed); g_free(fix);
}

/* Committed word delivered from gtv_tsf.dll (takes ownership). Feeds the
 * same AI typo check the hook engine used to trigger. */
void gtv_tray_ai_word(gchar *word){
    if (!word || !*word) { g_free(word); return; }
    InterlockedIncrement(&ai_word_generation);
    if (g_app.config.spellcheck && g_app.config.ai_enabled && gtv_tray_should_check(word))
        gtv_tray_check_spelling_async(word);
    g_free(word);
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

gboolean gtv_tray_should_check(const gchar *word){
    if(!word || g_utf8_strlen(word, -1) < 2) return FALSE;
    if(!spell_word_valid(word)) return TRUE;
    EnterCriticalSection(&learned_lock);
    w_learned_ensure();
    gchar *fix = gtv_fixes_lookup(w_learned_fixes, word);
    LeaveCriticalSection(&learned_lock);
    gboolean hit = fix && *fix;
    g_free(fix);
    return hit;
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
