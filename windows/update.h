#ifndef UPDATE_H
#define UPDATE_H

#include <windows.h>
#include <glib.h>

#define WM_GTV_UPDATE_RESULT (WM_APP + 101)
#define WM_GTV_UPDATE_DOWNLOADED (WM_APP + 102)

/* Update-check result passed from the worker thread to the UI thread. */
typedef struct { gint status; gchar *tag; gchar *url; } GtvUpdateResult;

/* Async GitHub release check; posts WM_GTV_UPDATE_RESULT to notify.
 * When manual is FALSE the 24h throttle applies (auto-check at startup). */
void gtv_update_check_async(HWND notify, gboolean manual);
/* Handle WM_GTV_UPDATE_RESULT (frees res). Shows a balloon when an update
 * is available, or a status balloon for manual checks. */
void gtv_update_on_result(GtvUpdateResult *res, gboolean manual);
/* TRUE when the latest balloon belongs to the updater (route clicks here).
 * Starts the download; WM_GTV_UPDATE_DOWNLOADED runs the installer. */
gboolean gtv_update_balloon_clicked(void);
void gtv_update_on_downloaded(gchar *installer_path);
/* Called when another feature takes over the shared balloon slot. */
void gtv_update_disown_balloon(void);

#endif /* UPDATE_H */
