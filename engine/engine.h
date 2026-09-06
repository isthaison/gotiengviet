#ifndef GTV_ENGINE_H
#define GTV_ENGINE_H
#include <glib.h>

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
    GPtrArray *suggestions;
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
/* Ollama is optional; unavailable/invalid responses fall back to spelling rules. */
gboolean gtv_ai_available(const GtvConfig *config);
GPtrArray *gtv_ai_suggest(const GtvConfig *config, const gchar *word, const gchar *context);
/* Vector-based semantic next-word prediction based on sentence context and optional prefix. */
GPtrArray *gtv_vector_predict_next(const gchar *context, const gchar *prefix, guint max_results);
GPtrArray *gtv_predict_next(const GtvConfig *config, const gchar *context, const gchar *prefix);
#endif
