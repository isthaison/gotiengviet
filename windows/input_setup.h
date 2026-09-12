#ifndef GTV_INPUT_SETUP_H
#define GTV_INPUT_SETUP_H

#include <glib.h>

/* Attach the GoTV TIP to the user's existing languages (vi -> 042a TIP,
 * en-US -> 0409 TIP), purely additive and idempotent. Runs in a worker
 * thread; safe to call at every startup (a version stamp skips repeats).
 * Uses the OS-validated Set-WinUserLanguageList path via a hidden
 * powershell child. Machine registration (--register-tsf, admin) must
 * have run first, otherwise Windows drops the TIP and we retry later. */
void gtv_input_setup_ensure_async(void);

#endif /* GTV_INPUT_SETUP_H */
