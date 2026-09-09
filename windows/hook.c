#include "hook.h"
#include "tray.h"
#include "internal.h"
#include <glib/gstdio.h>
#include <stdio.h>

/* Mirror of the current word on screen: pass-through clients show raw
 * keys immediately, so commits and resends must erase exactly this
 * before writing. Initialized with the keyboard hook (same thread). */
static GtvMirror s_mirror;
static gboolean s_mirror_ready = FALSE;
/* Last foreground window seen: a change abandons the composition, so a
 * commit can never erase text in the wrong window. */
static HWND s_last_fg = NULL;

void gtv_hook_send_backspaces(int count) {
    if (count <= 0) return;
    INPUT *inputs = g_new0(INPUT, count * 2);
    for (int i = 0; i < count; i++) {
        inputs[i * 2].type = INPUT_KEYBOARD;
        inputs[i * 2].ki.wVk = VK_BACK;
        inputs[i * 2].ki.dwExtraInfo = GTV_HOOK_MAGIC;

        inputs[i * 2 + 1].type = INPUT_KEYBOARD;
        inputs[i * 2 + 1].ki.wVk = VK_BACK;
        inputs[i * 2 + 1].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[i * 2 + 1].ki.dwExtraInfo = GTV_HOOK_MAGIC;
    }
    SendInput(count * 2, inputs, sizeof(INPUT));
    g_free(inputs);
}

static void send_unicode_string(const wchar_t *wstr) {
    if (!wstr || !*wstr) return;
    int len = (int)wcslen(wstr);
    INPUT *inputs = g_new0(INPUT, len * 2);
    for (int i = 0; i < len; i++) {
        inputs[i * 2].type = INPUT_KEYBOARD;
        inputs[i * 2].ki.wScan = (WORD)wstr[i];
        inputs[i * 2].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[i * 2].ki.dwExtraInfo = GTV_HOOK_MAGIC;

        inputs[i * 2 + 1].type = INPUT_KEYBOARD;
        inputs[i * 2 + 1].ki.wScan = (WORD)wstr[i];
        inputs[i * 2 + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs[i * 2 + 1].ki.dwExtraInfo = GTV_HOOK_MAGIC;
    }
    SendInput(len * 2, inputs, sizeof(INPUT));
    g_free(inputs);
}

/* After a word-ending commit (word + delimiter), ask Ollama about likely
 * typos in the background. Offline-safe: the local syllable check gates. */
static void maybe_ai_suggest(const gchar *commit) {
    if (!g_app.config.spellcheck || !g_app.config.ai_enabled) return;
    if (!commit) return;
    glong len = g_utf8_strlen(commit, -1);
    if (len < 3) return;
    gchar *end = g_utf8_offset_to_pointer(commit, len - 1);
    gunichar delim = g_utf8_get_char(end);
    if (!g_unichar_isspace(delim) && !g_unichar_ispunct(delim)) return;
    gchar *word = g_utf8_substring(commit, 0, len - 1);
    if (gtv_tray_should_check(word))
        gtv_tray_check_spelling_async(word);
    g_free(word);
}

void gtv_hook_send_text(const gchar *utf8) {
    if (!utf8 || !*utf8) return;
    glong wlen = 0;
    guint16 *wstr = g_utf8_to_utf16(utf8, -1, NULL, &wlen, NULL);
    if (wstr) {
        send_unicode_string((const wchar_t *)wstr);
        g_free(wstr);
    }
}

void gtv_hook_reset_buffer(void) {
    if (g_app.engine) {
        gtv_engine_reset(g_app.engine);
    }
    if (s_mirror_ready) {
        gtv_mirror_clear(&s_mirror);
    }
}

static gchar *windows_config_path(void) {
    return g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
}

void gtv_hook_save_enabled(void) {
    gchar *path = windows_config_path();
    GKeyFile *kf = g_key_file_new();
    g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
    g_key_file_set_boolean(kf, "input", "enabled", g_app.enabled);
    gchar *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    g_key_file_save_to_file(kf, path, NULL);
    g_key_file_unref(kf);
    g_free(path);
}

gboolean gtv_hook_load_enabled(gboolean def) {
    gchar *path = windows_config_path();
    GKeyFile *kf = g_key_file_new();
    gboolean enabled = def;
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)
        && g_key_file_has_key(kf, "input", "enabled", NULL))
        enabled = g_key_file_get_boolean(kf, "input", "enabled", NULL);
    g_key_file_unref(kf);
    g_free(path);
    return enabled;
}

void gtv_hook_set_mode(gboolean enabled) {
    g_app.enabled = enabled;
    gtv_hook_reset_buffer();
    gtv_tray_update_icon(g_app.enabled);
    gtv_hook_save_enabled();
}

void gtv_hook_toggle_mode(void) {
    gtv_hook_set_mode(!g_app.enabled);
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lParam;

    /* Ignore events injected by our own SendInput */
    if (kbd->dwExtraInfo == GTV_HOOK_MAGIC) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    /* Window switch abandons the composition: the screen word tracked
     * below belongs to the old window and must never be erased over. */
    {
        HWND fg = GetForegroundWindow();
        if (fg != s_last_fg) {
            s_last_fg = fg;
            gtv_hook_reset_buffer();
        }
    }

    /* Hotkey detection: Ctrl + Shift or Alt + Z toggles mode */
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        g_app.last_input_tick = GetTickCount();
        gboolean ctrl_down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        gboolean shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        gboolean alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

        if ((kbd->vkCode == VK_SHIFT && ctrl_down) ||
            (kbd->vkCode == VK_CONTROL && shift_down) ||
            (kbd->vkCode == 'Z' && alt_down)) {
            /* Debounce: holding the hotkey auto-repeats keydown. */
            static DWORD last_toggle = 0;
            DWORD now = GetTickCount();
            if (now - last_toggle > 500) {
                last_toggle = now;
                gtv_hook_toggle_mode();
            }
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
    }

    /* If in English mode, do not process */
    if (!g_app.enabled) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    /* Navigation keys, modifiers, enter, tab, escape reset composition */
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        if (kbd->vkCode == VK_RETURN || kbd->vkCode == VK_ESCAPE || kbd->vkCode == VK_TAB ||
            kbd->vkCode == VK_LEFT || kbd->vkCode == VK_RIGHT || kbd->vkCode == VK_UP || kbd->vkCode == VK_DOWN ||
            kbd->vkCode == VK_HOME || kbd->vkCode == VK_END || kbd->vkCode == VK_PRIOR || kbd->vkCode == VK_NEXT) {
            gtv_hook_reset_buffer();
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        /* Modifiers like Ctrl+C, Ctrl+V, Alt+F4 reset buffer */
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000) ||
            (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) {
            gtv_hook_reset_buffer();
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        /* Backspace handling: engine pops one unit in parallel with the
         * app erasing one screen char; mirror follows the screen. */
        if (kbd->vkCode == VK_BACK) {
            guint backspaces = 0;
            gtv_engine_process(g_app.engine, '\b', &backspaces);
            if (s_mirror_ready) {
                gtv_mirror_backspaced(&s_mirror);
            }
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        /* Convert vkCode to Unicode character */
        BYTE key_state[256];
        GetKeyboardState(key_state);
        key_state[VK_SHIFT] = (BYTE)((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 0x80 : 0);
        key_state[VK_CAPITAL] = (BYTE)(GetKeyState(VK_CAPITAL) & 1);
        key_state[VK_CONTROL] = 0;
        key_state[VK_MENU] = 0;

        WCHAR wchars[4] = {0};
        HWND foreground = GetForegroundWindow();
        DWORD thread_id = foreground ? GetWindowThreadProcessId(foreground, NULL) : 0;
        HKL layout = GetKeyboardLayout(thread_id);
        int count = ToUnicodeEx(kbd->vkCode, kbd->scanCode, key_state, wchars, 4, 0, layout);

        if (count == 1 && wchars[0] >= 0x20) {
            /* Mirror decides: plain appends pass through untouched (real
             * scancodes reach the app), while compositions and commits
             * first erase exactly what is on screen. This fixes both the
             * space duplication (commit re-typed the visible word) and
             * the mid-word desync (e.g. '['). */
            gchar *raw_utf8 = g_utf16_to_utf8(wchars, 1, NULL, NULL, NULL);
            gunichar ch = raw_utf8 ? g_utf8_get_char_validated(raw_utf8, -1) : (gunichar)-1;
            if (!raw_utf8 || ch == (gunichar)-1 || ch == (gunichar)-2) {
                g_free(raw_utf8);
                gtv_hook_reset_buffer();
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }
            guint backspaces = 0;
            gchar *commit = gtv_engine_process(g_app.engine, ch, &backspaces);
            (void)backspaces; /* erasure is driven by the mirror, not this */
            gchar *buffer = gtv_engine_buffer(g_app.engine);
            GtvMirrorAction act = s_mirror_ready
                ? gtv_mirror_decide(&s_mirror, buffer, raw_utf8, commit)
                : (commit ? GTV_MIRROR_COMMIT : GTV_MIRROR_PASS);
            if (act == GTV_MIRROR_COMMIT) {
                /* Minimal edit: an unchanged word needs zero Backspaces,
                 * which also keeps autocomplete-driven fields (Chrome
                 * omnibox swallows the first Backspace to dismiss its
                 * suggestion) exact. */
                guint erase = 0;
                const gchar *send_from = commit ? commit : "";
                gtv_mirror_diff(&s_mirror, send_from, &erase, &send_from);
                gtv_hook_send_backspaces((int)erase);
                gtv_hook_send_text(send_from);
                gtv_mirror_committed(&s_mirror);
                if (commit) {
                    maybe_ai_suggest(commit);
                }
                g_free(commit);
                g_free(buffer);
                g_free(raw_utf8);
                return 1; /* Suppress original key */
            }
            if (act == GTV_MIRROR_RESEND) {
                guint erase = 0;
                const gchar *send_from = buffer;
                gtv_mirror_diff(&s_mirror, buffer, &erase, &send_from);
                gtv_hook_send_backspaces((int)erase);
                gtv_hook_send_text(send_from);
                gtv_mirror_resent(&s_mirror, buffer);
                g_free(commit);
                g_free(buffer);
                g_free(raw_utf8);
                return 1; /* Suppress original key */
            }
            if (s_mirror_ready) {
                gtv_mirror_passthrough(&s_mirror, ch);
            }
            g_free(commit);
            g_free(buffer);
            g_free(raw_utf8);
            /* Fall through: the raw key goes to the app. */
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

/* A click may move the caret: abandon the composition (buffer and
 * screen mirror) so later commits can never erase text elsewhere. */
static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT *ms = (MSLLHOOKSTRUCT *)lParam;
        if (ms->dwExtraInfo != GTV_HOOK_MAGIC &&
            (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
             wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN)) {
            gtv_hook_reset_buffer();
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

gboolean gtv_hook_install(void) {
    if (g_app.keyboard_hook) return TRUE;
    if (!s_mirror_ready) {
        gtv_mirror_init(&s_mirror);
        s_mirror_ready = TRUE;
    }
    HINSTANCE hinst = GetModuleHandle(NULL);
    g_app.keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hinst, 0);
    if (g_app.keyboard_hook) {
        g_app.mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hinst, 0);
    }
    return g_app.keyboard_hook != NULL;
}

void gtv_hook_uninstall(void) {
    if (g_app.mouse_hook) {
        UnhookWindowsHookEx(g_app.mouse_hook);
        g_app.mouse_hook = NULL;
    }
    if (g_app.keyboard_hook) {
        UnhookWindowsHookEx(g_app.keyboard_hook);
        g_app.keyboard_hook = NULL;
    }
}
