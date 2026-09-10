#include "tsf_service.h"

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

    // Convert key to Unicode to support letters, numbers, and punctuation (: ; < > for emojis)
    BYTE key_state[256];
    GetKeyboardState(key_state);
    WCHAR wchars[4] = {0};
    HKL layout = GetKeyboardLayout(0);
    int count = ToUnicodeEx((UINT)wParam, (UINT)((lParam >> 16) & 0xFF), key_state, wchars, 4, 0, layout);
    if (count == 1 && wchars[0] >= 0x20) {
        *pfEaten = TRUE;
        return S_OK;
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

    // Space or delimiter handling
    if (g_unichar_isspace(ch) || g_unichar_ispunct(ch)) {
        if (IsComposing()) {
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
                g_free(commit);
            } else {
                EndComposition(pic, TRUE);
            }
            gtv_engine_reset(m_pEngine);
            /* The delimiter is already inside the committed composition
             * text above: eat the key or it inserts a second one. */
            *pfEaten = TRUE;
            return S_OK;
        }
        return S_OK;
    }

    // Normal character handling
    guint bs = 0;
    gchar *commit = gtv_engine_process(m_pEngine, ch, &bs);
    if (commit) {
        // Word committed, start fresh
        glong wlen = 0;
        wchar_t *wcommit = (wchar_t*)g_utf8_to_utf16(commit, -1, NULL, &wlen, NULL);
        if (wcommit) {
            UpdateCompositionText(pic, wcommit, (int)wlen);
            g_free(wcommit);
        }
        EndComposition(pic, TRUE);
        g_free(commit);

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
        }
        g_free(buf);
    }

    *pfEaten = TRUE;
    return S_OK;
}
