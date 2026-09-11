#ifndef GTV_STARTUP_H
#define GTV_STARTUP_H

#include <windows.h>
#include <glib.h>

/* How GoTiengViet starts with Windows. USER = HKCU Run value (no UAC,
 * cannot reach elevated windows). ADMIN = Task Scheduler logon task with
 * highest privileges (needs one UAC consent to set up), so typing helpers
 * keep working inside admin windows. */
typedef enum { GTV_STARTUP_NONE, GTV_STARTUP_USER, GTV_STARTUP_ADMIN } GtvStartupMode;

GtvStartupMode gtv_startup_get(void);
/* Elevation-free. Turning USER on while ADMIN is active is a no-op here:
 * use gtv_startup_request() so the worker can drop the admin task first. */
void gtv_startup_set_user(gboolean enable);
/* Ask Windows for elevation (UAC) and let the elevated worker apply it:
 * "admin" enables the admin task, "admin-off" removes it, "user" switches
 * back to user-level startup. hwnd is only used for error UI, may be NULL. */
void gtv_startup_request(HWND hwnd, const char *op);
/* Elevated worker for --configure-startup admin|admin-off|user.
 * Returns the process exit code (0 ok). Shows its own error dialog. */
int gtv_startup_apply(const char *op);
gboolean gtv_startup_is_elevated(void);

#endif /* GTV_STARTUP_H */
