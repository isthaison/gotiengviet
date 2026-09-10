#include "tsf_service.h"
#include "../app.h"

/* Report a committed word to the tray app for the AI typo check. The tray
 * owns suggestions/balloons/learning; TSF just types. Best-effort: the
 * tray may not run (portable use), and must never block typing. */
static void NotifyTrayWord(const char *word) {
    if (!word || !*word) return;
    HWND tray = FindWindowA(GTV_TRAY_WINDOW_CLASS, NULL);
    if (!tray) return;
    COPYDATASTRUCT cds;
    cds.dwData = (ULONG_PTR)GTV_AI_COPYDATA_ID;
    cds.cbData = (DWORD)strlen(word) + 1;
    cds.lpData = (PVOID)word;
    SendMessageTimeoutA(tray, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG, 500, NULL);
}

STDMETHODIMP CGtvTextService::OnSetFocus(BOOL fForeground)
{
    return S_OK;
}

STDMETHODIMP CGtvTextService::OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten)
{
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CGtvTextService::OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CGtvTextService::OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

static inline gboolean is_emoji_starter(gunichar ch) {
    return (ch == ':' || ch == ';' || ch == '<' || ch == '(');
}

static inline gboolean is_telex_shortcut(GtvEngine *engine, gunichar ch) {
    return (engine && engine->mode == GTV_TELEX &&
            (ch == '[' || ch == ']' || ch == '{' || ch == '}'));
}

STDMETHODIMP CGtvTextService::OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    if (!m_fEnabled) return S_OK;

    // Check modifiers
    if ((GetKeyState(VK_CONTROL) & 0x8000) ||
        (GetKeyState(VK_MENU) & 0x8000) ||
        (GetKeyState(VK_LWIN) & 0x8000) ||
        (GetKeyState(VK_RWIN) & 0x8000)) {
        return S_OK;
    }

    if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_BACK) {
        if (IsComposing()) *pfEaten = TRUE;
        return S_OK;
    }

    if (wParam == VK_SPACE) {
        if (IsComposing()) *pfEaten = TRUE;
        return S_OK;
    }

    // Convert key to Unicode
    BYTE key_state[256];
    GetKeyboardState(key_state);
    WCHAR wchars[4] = {0};
    HKL layout = GetKeyboardLayout(0);
    int count = ToUnicodeEx((UINT)wParam, (UINT)((lParam >> 16) & 0xFF), key_state, wchars, 4, 0, layout);
    if (count == 1 && wchars[0] >= 0x20) {
        gunichar ch = (gunichar)wchars[0];
        if (IsComposing()) {
            *pfEaten = TRUE;
            return S_OK;
        }
        if (g_unichar_isalnum(ch) || is_emoji_starter(ch) || is_telex_shortcut(m_pEngine, ch)) {
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    return S_OK;
}

STDMETHODIMP CGtvTextService::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    if (!m_fEnabled) return S_OK;
    if (!pic || !m_pEngine) return S_OK;

    // Modifiers reset composition
    if ((GetKeyState(VK_CONTROL) & 0x8000) ||
        (GetKeyState(VK_MENU) & 0x8000) ||
        (GetKeyState(VK_LWIN) & 0x8000) ||
        (GetKeyState(VK_RWIN) & 0x8000)) {
        if (IsComposing()) {
            EndComposition(pic, TRUE);
            gtv_engine_reset(m_pEngine);
        }
        return S_OK;
    }

    // Escape cancels composition
    if (wParam == VK_ESCAPE) {
        if (IsComposing()) {
            EndComposition(pic, FALSE);
            gtv_engine_reset(m_pEngine);
            *pfEaten = TRUE;
        }
        return S_OK;
    }

    // Return commits composition
    if (wParam == VK_RETURN) {
        if (IsComposing()) {
            EndComposition(pic, TRUE);
            gtv_engine_reset(m_pEngine);
            *pfEaten = TRUE;
        }
        return S_OK;
    }

    // Backspace handles in-composition backspacing
    if (wParam == VK_BACK) {
        if (IsComposing()) {
            guint bs = 0;
            gtv_engine_process(m_pEngine, '\b', &bs);
            gchar *buf = gtv_engine_buffer(m_pEngine);
            if (!buf || !*buf) {
                EndComposition(pic, FALSE);
            } else {
                glong wlen = 0;
                wchar_t *wbuf = (wchar_t*)g_utf8_to_utf16(buf, -1, NULL, &wlen, NULL);
                if (wbuf) {
                    UpdateCompositionText(pic, wbuf, (int)wlen);
                    g_free(wbuf);
                }
            }
            g_free(buf);
            *pfEaten = TRUE;
            return S_OK;
        }
        return S_OK;
    }

    // Convert virtual key to Unicode character
    BYTE key_state[256];
    GetKeyboardState(key_state);
    WCHAR wchars[4] = {0};
    HKL layout = GetKeyboardLayout(0);
    int count = ToUnicodeEx((UINT)wParam, (UINT)((lParam >> 16) & 0xFF), key_state, wchars, 4, 0, layout);

    if (count != 1 || wchars[0] < 0x20) {
        if (IsComposing()) {
            EndComposition(pic, TRUE);
            gtv_engine_reset(m_pEngine);
        }
        return S_OK;
    }

    gunichar ch = (gunichar)wchars[0];

    // If not composing, only start composition on alphanumeric, emoji starter, or Telex shortcut
    if (!IsComposing()) {
        if (!g_unichar_isalnum(ch) && !is_emoji_starter(ch) && !is_telex_shortcut(m_pEngine, ch)) {
            return S_OK; // Pass through to target application
        }
    }

    // Process character through engine
    guint bs = 0;
    gchar *commit = gtv_engine_process(m_pEngine, ch, &bs);
    if (commit) {
        glong wlen = 0;
        wchar_t *wcommit = (wchar_t*)g_utf8_to_utf16(commit, -1, NULL, &wlen, NULL);
        if (wcommit) {
            UpdateCompositionText(pic, wcommit, (int)wlen);
            g_free(wcommit);
        }
        EndComposition(pic, TRUE);

        /* AI check on the word without its trailing delimiter */
        const gchar *end = commit + strlen(commit);
        const gchar *prev = g_utf8_prev_char(end);
        if (prev > commit) {
            gchar *wordonly = g_strndup(commit, prev - commit);
            NotifyTrayWord(wordonly);
            g_free(wordonly);
        }
        g_free(commit);

        // If engine kept leftover buffer, start fresh composition
        gchar *buf = gtv_engine_buffer(m_pEngine);
        if (buf && *buf) {
            wlen = 0;
            wchar_t *wbuf = (wchar_t*)g_utf8_to_utf16(buf, -1, NULL, &wlen, NULL);
            if (wbuf) {
                UpdateCompositionText(pic, wbuf, (int)wlen);
                g_free(wbuf);
            }
        }
        g_free(buf);
    } else {
        // Update active composition
        gchar *buf = gtv_engine_buffer(m_pEngine);
        if (buf && *buf) {
            glong wlen = 0;
            wchar_t *wbuf = (wchar_t*)g_utf8_to_utf16(buf, -1, NULL, &wlen, NULL);
            if (wbuf) {
                UpdateCompositionText(pic, wbuf, (int)wlen);
                g_free(wbuf);
            }
        } else {
            EndComposition(pic, FALSE);
        }
        g_free(buf);
    }

    *pfEaten = TRUE;
    return S_OK;
}
