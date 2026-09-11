#ifndef GTV_OLLAMA_H
#define GTV_OLLAMA_H

#include <windows.h>
#include <glib.h>

#define WM_GTV_OLLAMA_STATUS (WM_APP + 21)

/* %TEMP%\ollama_serve.log (Linux: /tmp/ollama_serve.log). Caller frees. */
gchar *gtv_ollama_log_path(void);

/* Append one UTF-8 line to the serve log (best-effort). */
void gtv_ollama_log(const gchar *line);

/* Full chain (like Linux): install Ollama when missing, then serve, then
 * pull the model. All progress goes to the serve log. */
void gtv_ollama_ensure_all_async(HWND hwnd, const gchar *url, const gchar *model);

/* Serve-only path (skips install/model via "rule"). */
void gtv_ollama_ensure_serve_async(const gchar *url);

/* Fire-and-forget /api/tags probe; posts WM_GTV_OLLAMA_STATUS(ok) to hwnd. */
void gtv_ollama_probe_async(HWND hwnd, const gchar *url);

/* Download progress query (-1 when already installed). */
gint64 gtv_ollama_download_bytes(void);

#endif /* GTV_OLLAMA_H */
