#ifndef GTV_TSF_SUGGEST_H
#define GTV_TSF_SUGGEST_H

#include <windows.h>
#include <msctf.h>
#include "engine.h"

class CGtvTextService;

/* Inline keyword suggestions for the Windows TSF service (parity with the
 * Linux IBus lookup table): while composing, a small popup near the caret
 * offers up to 5 candidates from Ollama (350 ms debounce), falling back to
 * offline learned data; ":xx" prefixes offer emoji instantly without AI.
 *
 * Threading: everything UI lives on the app thread that owns the key
 * events (one popup window per thread, recreated on thread change). Only
 * the Ollama fetch runs on a worker thread (synchronous engine API +
 * GCancellable); results come back via PostMessage and are dropped when
 * stale (generation counter + live-buffer match, like IBus pending_query).
 *
 * Selection keys (mirror IBus): Up/Down navigate, 1-5 accept (Telex mode
 * only; in VNI digits are tone keys), Tab accepts the highlight (exact
 * macro/emoji match wins first), Enter commits the highlight, Esc dismisses
 * (keeps composing), Space commits literally. Caller g_free is NOT needed:
 * all strings are copied internally.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* Called on the app thread after a printable/backspace key was processed
 * (and after Space/Return/Tab/macro commits to hide). Restarts the debounce
 * or shows emoji suggestions immediately. Safe to call when hidden. */
void gtv_suggest_on_key(CGtvTextService *service, ITfContext *pic);

/* Hide + cancel everything. Called on commits, focus loss, Deactivate,
 * OnCompositionTerminated and modifiers. */
void gtv_suggest_hide(void);

/* Popup state for key routing (app thread). */
gboolean gtv_suggest_visible(void);
gint gtv_suggest_count(void);

/* Navigation/selection (app thread, no-ops when hidden). */
void gtv_suggest_move_cursor(gint delta);
/* Tab semantics: replace the buffer with the highlight, keep composing. */
void gtv_suggest_accept_cursor(CGtvTextService *service, ITfContext *pic);
/* Enter/digit/click semantics: commit the highlight. */
void gtv_suggest_commit_cursor(CGtvTextService *service, ITfContext *pic);
void gtv_suggest_accept_index(CGtvTextService *service, ITfContext *pic, gint index);

/* Exact macro/emoji match for the live buffer (Tab keeps macro priority).
 * TRUE when Tab must take the engine macro path instead of the highlight. */
gboolean gtv_suggest_has_exact_match(CGtvTextService *service);

#ifdef __cplusplus
}
#endif

#endif /* GTV_TSF_SUGGEST_H */
