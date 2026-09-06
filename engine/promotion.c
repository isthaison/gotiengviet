#include "internal.h"

/* Phonological pairs that acquire a circumflex when the nucleus has a tail.
 * Only unmarked shapes are promoted; explicit horns/breves are never replaced. */
void auto_promote_diphthong(GArray *buf) {
    if (buf->len < 3) return;
    static const struct {gunichar first,last,tail; gboolean plain_first;} pairs[] = {
        {'i','e','u',FALSE}, {'y','e','u',FALSE}, {'u','o','i',TRUE}
    };
    int last_v=-1;
    for (int i=(int)buf->len-1;i>=0;i--)
        if (is_vowel(g_array_index(buf,gunichar,i))) {last_v=i;break;}
    for (guint i=0;i<G_N_ELEMENTS(pairs);i++) {
        int pos=last_v;
        if (last_v == (int)buf->len-1) {
            if (bare_lower(g_array_index(buf,gunichar,last_v)) != pairs[i].tail) continue;
            pos--;
        }
        if (pos < 1) continue;
        gunichar first=g_array_index(buf,gunichar,pos-1), last=g_array_index(buf,gunichar,pos);
        if (bare_lower(first) != pairs[i].first || bare_lower(last) != pairs[i].last ||
            get_diac(last) != DIAC_NONE || (pairs[i].plain_first && get_diac(first) != DIAC_NONE)) continue;
        gunichar changed;
        if (lookup_char(pairs[i].last,DIAC_CIRCUMFLEX,get_tone(last),g_unichar_isupper(last),&changed))
            g_array_index(buf,gunichar,pos)=changed;
        return;
    }
}
