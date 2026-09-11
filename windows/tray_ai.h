#ifndef GTV_TRAY_AI_H
#define GTV_TRAY_AI_H

#include <glib.h>

typedef struct { gchar *typed; gchar *fix; } GtvAiResult;

void gtv_tray_ai_init(void);
void gtv_tray_ai_cleanup(void);
gboolean gtv_tray_should_check(const gchar *word);
void gtv_tray_suggest_balloon(const gchar *typed, const gchar *correction);
void gtv_tray_apply_pending(void);
void gtv_tray_ai_word(gchar *word);
void gtv_tray_check_spelling_async(const gchar *word);

#endif /* GTV_TRAY_AI_H */
