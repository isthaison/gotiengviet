/* Versioned install layout helpers (portable, glib-only).
 *
 * New-world Windows layout under {app}:
 *   gtv_tsf.dll          stable TSF stub (registered once, path never changes)
 *   current.txt          current payload version, e.g. "0.8.12"
 *   ver\<V>\            payload: tray exe, gtv_engine.dll, runtime, data\
 *
 * Startup entries point at a versioned tray exe; a stale one self-forwards
 * to the current payload (gtv_forward_target). No separate launcher binary:
 * tiny forwarder exes get eaten by ML heuristics (Heur.AdvML.D).
 *
 * Rule: if the module lives at {app}\ver\<V>\..., appdir is two levels up;
 * otherwise appdir is the module's own dir (dev builds, tests). When
 * current.txt is absent the module dir itself is the payload dir.
 */
#ifndef GTV_VERDIR_H
#define GTV_VERDIR_H

#include <glib.h>

G_BEGIN_DECLS

/* App root for a module path (may be NULL-terminated UTF-8, any separators).
 * Never returns NULL (falls back to "." or the dirname). Caller g_free. */
gchar *gtv_app_dir_for_module(const gchar *modpath);

/* Contents of {appdir}/current.txt stripped, or NULL. Caller g_free. */
gchar *gtv_ver_current(const gchar *appdir);

/* Payload dir: {appdir}/ver/<ver>, or a copy of appdir when ver is NULL
 * (dev layout without current.txt). Caller g_free. */
gchar *gtv_ver_dir(const gchar *appdir, const gchar *ver);

/* Tray exe of a NEWER payload to forward to: {appdir}/ver/<current>/
 * gotiengviet.exe when current.txt names a version strictly newer than
 * own_ver (stale shortcut/Run/self-heal). NULL when current, missing or
 * unreadable (dev/legacy layouts). Caller g_free. */
gchar *gtv_forward_target(const gchar *appdir, const gchar *own_ver);

/* Numeric X.Y.Z compare (missing parts are 0). <0/0/>0 like strcmp. */
gint gtv_ver_cmp(const gchar *a, const gchar *b);

/* Delete ver\<V> payload dirs under appdir except current and the newest
 * other one (keep at most 2). Best-effort: locked trees are skipped and
 * retried on the next launch. Returns number of payloads removed. */
guint gtv_ver_cleanup(const gchar *appdir);

/* Remove flat-layout leftovers after migration to ver\ (best-effort).
 * Only acts when ver\ + current.txt exist. Locked files are skipped and
 * retried on the next launch. */
void gtv_legacy_cleanup(const gchar *appdir);

G_END_DECLS

#endif /* GTV_VERDIR_H */
