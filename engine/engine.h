#ifndef GTV_ENGINE_H
#define GTV_ENGINE_H
#include <glib.h>
#include <gio/gio.h>

/* Returned strings and arrays belong to the caller (g_free/g_ptr_array_unref). */
typedef enum { GTV_TELEX, GTV_VNI } GtvMode;
typedef struct {
    GtvMode mode;
    gboolean modern, spellcheck, ai_enabled;
    gchar *model, *url, *port;
} GtvConfig;
typedef struct {
    GtvMode mode;
    gboolean modern, spellcheck;
    GArray *buffer;
} GtvEngine;

void gtv_init(void);
gchar *gtv_transform(const gchar *input, GtvMode mode, gboolean modern);
GtvEngine *gtv_engine_new(const GtvConfig *config);
void gtv_engine_free(GtvEngine *engine);
void gtv_engine_reset(GtvEngine *engine);
gchar *gtv_engine_buffer(const GtvEngine *engine);
/* NULL means composing; otherwise returns committed text and clears the buffer. */
gchar *gtv_engine_process(GtvEngine *engine, gunichar key, guint *backspaces);
void gtv_config_load(GtvConfig *config, const gchar *directory);
gboolean gtv_config_save(const GtvConfig *config, const gchar *directory, GError **error);
void gtv_config_clear(GtvConfig *config);
/* Ollama-only suggestions; disabled/unavailable providers return no candidates. */
gboolean gtv_ai_available(const GtvConfig *config);
GPtrArray *gtv_ai_suggest(const GtvConfig *config, const gchar *word, const gchar *context);
GPtrArray *gtv_predict_next(const GtvConfig *config, const gchar *context, const gchar *prefix);
/* Self-update against GitHub releases. Check/download need curl and network;
 * parse/compare/throttle are offline. Out strings belong to the caller. */
typedef enum { GTV_UPDATE_AVAILABLE, GTV_UPDATE_CURRENT, GTV_UPDATE_ERROR } GtvUpdateStatus;
#define GTV_GITHUB_REPO "isthaison/gotiengviet"
gint gtv_version_compare(const gchar *a, const gchar *b);
gchar *gtv_update_asset_name(const gchar *version);
GtvUpdateStatus gtv_update_parse_release(const gchar *json, const gchar *current_version,
                                         gchar **out_tag, gchar **out_asset_url);
GtvUpdateStatus gtv_update_check(const gchar *repo, const gchar *current_version,
                                 gchar **out_tag, gchar **out_asset_url);
gboolean gtv_update_download(const gchar *url, const gchar *dest_path);
gboolean gtv_update_should_autocheck(void);
void gtv_update_mark_checked(void);
/* Screen mirror for pass-through clients (Windows hook): tracks what the
 * client should be showing for the current word, so commits and resends
 * erase exactly that. IBus uses preedit instead and ignores this. */
typedef struct { GString *shown; } GtvMirror;
typedef enum { GTV_MIRROR_PASS, GTV_MIRROR_RESEND, GTV_MIRROR_COMMIT } GtvMirrorAction;
GtvMirror *gtv_mirror_new(void);
void gtv_mirror_init(GtvMirror *m);
void gtv_mirror_clear(GtvMirror *m);
void gtv_mirror_free(GtvMirror *m);
void gtv_mirror_passthrough(GtvMirror *m, gunichar raw);
void gtv_mirror_backspaced(GtvMirror *m);
GtvMirrorAction gtv_mirror_decide(GtvMirror *m, const gchar *buffer, const gchar *raw_utf8, const gchar *commit);
guint gtv_mirror_erase_count(GtvMirror *m);
void gtv_mirror_diff(GtvMirror *m, const gchar *new_text, guint *erase_chars, const gchar **send_from);
void gtv_mirror_resent(GtvMirror *m, const gchar *buffer);
void gtv_mirror_committed(GtvMirror *m);
#endif
