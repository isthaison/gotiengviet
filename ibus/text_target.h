#ifndef GTV_TEXT_TARGET_H
#define GTV_TEXT_TARGET_H
#include <atspi/atspi.h>
typedef struct { AtspiText *text; gchar *expected; } GtvTextTarget;
gboolean gtv_text_range(const gchar *text,gint cursor,gint anchor,const gchar *original,gint *start,gint *end);
GtvTextTarget *gtv_text_target_select(const gchar *original,const gchar *replacement);
const gchar *gtv_text_target_status(void);
gboolean gtv_text_target_verify(GtvTextTarget *target);
void gtv_text_target_free(GtvTextTarget *target);
#endif
