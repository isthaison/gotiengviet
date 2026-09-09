/* Screen mirror for pass-through clients (e.g. the Windows hook): the
 * client shows raw keys immediately, so the hook must erase exactly what
 * is on screen before resending a composition or committing a word.
 * Pure logic, no OS calls; unit-tested on Linux. IBus uses preedit
 * instead and never touches this. */
#include "internal.h"

GtvMirror *gtv_mirror_new(void) {
    GtvMirror *m = g_new0(GtvMirror, 1);
    m->shown = g_string_new("");
    return m;
}
void gtv_mirror_init(GtvMirror *m) {
    m->shown = g_string_new("");
}
void gtv_mirror_clear(GtvMirror *m) {
    if (m->shown) g_string_truncate(m->shown, 0);
}
void gtv_mirror_free(GtvMirror *m) {
    if (!m) return;
    if (m->shown) g_string_free(m->shown, TRUE);
    g_free(m);
}
void gtv_mirror_passthrough(GtvMirror *m, gunichar raw) {
    g_string_append_unichar(m->shown, raw);
}
void gtv_mirror_backspaced(GtvMirror *m) {
    if (!m->shown->len) return;
    gchar *prev = g_utf8_prev_char(m->shown->str + m->shown->len);
    g_string_truncate(m->shown, prev - m->shown->str);
}
GtvMirrorAction gtv_mirror_decide(GtvMirror *m, const gchar *buffer, const gchar *raw_utf8, const gchar *commit) {
    if (commit) return GTV_MIRROR_COMMIT;
    if (!buffer) buffer = "";
    if (!raw_utf8) raw_utf8 = "";
    gchar *expected = g_strconcat(m->shown->str, raw_utf8, NULL);
    gboolean same = !strcmp(expected, buffer);
    g_free(expected);
    return same ? GTV_MIRROR_PASS : GTV_MIRROR_RESEND;
}
guint gtv_mirror_erase_count(GtvMirror *m) {
    return (guint)g_utf8_strlen(m->shown->str, -1);
}
void gtv_mirror_resent(GtvMirror *m, const gchar *buffer) {
    g_string_assign(m->shown, buffer ? buffer : "");
}
void gtv_mirror_committed(GtvMirror *m) {
    g_string_truncate(m->shown, 0);
}
