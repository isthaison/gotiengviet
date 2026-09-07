#include "hook.h"
#include "tray.h"
#include "internal.h"
#include <glib/gstdio.h>
#include <stdio.h>

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
    if (g_utf8_strlen(word, -1) >= 2 && !spell_word_valid(word))
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

        /* Backspace handling */
        if (kbd->vkCode == VK_BACK) {
            guint backspaces = 0;
            gtv_engine_process(g_app.engine, '\b', &backspaces);
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
            gunichar ch = (gunichar)wchars[0];
            guint backspaces = 0;
            gchar *commit = gtv_engine_process(g_app.engine, ch, &backspaces);

            if (commit) {
                if (backspaces > 0) {
                    gtv_hook_send_backspaces((int)backspaces);
                }
                glong wlen = 0;
                guint16 *wstr = g_utf8_to_utf16(commit, -1, NULL, &wlen, NULL);
                if (wstr) {
                    send_unicode_string((const wchar_t *)wstr);
                    g_free(wstr);
                }
                maybe_ai_suggest(commit);
                g_free(commit);
                return 1; /* Suppress original key */
            } else if (backspaces > 0) {
                /* Buffer modified in place (e.g. aa -> â, as -> á) */
                gchar *current = gtv_engine_buffer(g_app.engine);
                gtv_hook_send_backspaces((int)backspaces);
                glong wlen = 0;
                guint16 *wstr = g_utf8_to_utf16(current, -1, NULL, &wlen, NULL);
                if (wstr) {
                    send_unicode_string((const wchar_t *)wstr);
                    g_free(wstr);
                }
                g_free(current);
                return 1; /* Suppress original key */
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

gboolean gtv_hook_install(void) {
    if (g_app.keyboard_hook) return TRUE;
    HINSTANCE hinst = GetModuleHandle(NULL);
    g_app.keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hinst, 0);
    return g_app.keyboard_hook != NULL;
}

void gtv_hook_uninstall(void) {
    if (g_app.keyboard_hook) {
        UnhookWindowsHookEx(g_app.keyboard_hook);
        g_app.keyboard_hook = NULL;
    }
}
