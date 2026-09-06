#include "internal.h"

/* Select a tone-bearing vowel from the nucleus. Shape priority and vowel-pair
 * policies are data, shared by every key mapping and every typing order. */
int find_tone_position(GArray *word, gboolean modern) {
    if (!word->len) return -1;
    int *positions = g_new(int, word->len);
    guint count = 0;
    for (guint i=0;i<word->len;i++)
        if (is_vowel(g_array_index(word,gunichar,i))) positions[count++] = i;
    if (!count) { g_free(positions); return -1; }
    if (count > 1 && positions[0] == 1) {
        gunichar onset=bare_lower(g_array_index(word,gunichar,0));
        gunichar vowel=bare_lower(g_array_index(word,gunichar,1));
        if ((onset == 'q' && vowel == 'u') || (onset == 'g' && vowel == 'i')) {
            memmove(positions,positions+1,(--count)*sizeof(int));
        }
    }
    static const int shape_priority[] = {0,1,2,1,0};
    int best=0, result=positions[0];
    for (guint i=0;i<count;i++) {
        int priority=shape_priority[get_diac(g_array_index(word,gunichar,positions[i]))];
        if (priority && priority >= best) {best=priority;result=positions[i];}
    }
    if (!best && count > 1) {
        gboolean closed=positions[count-1] < (int)word->len-1;
        guint target=count == 3 ? 1 : 0;
        if (count == 2) {
            gunichar first=bare_lower(g_array_index(word,gunichar,positions[0]));
            gunichar last=bare_lower(g_array_index(word,gunichar,positions[1]));
            static const struct {gunichar first,last; gboolean modern_only;} pairs[] = {
                {'o','a',TRUE},{'o','e',TRUE},{'u','y',TRUE},
                {'i','e',FALSE},{'y','e',FALSE},{'u','o',FALSE},{'o','o',FALSE}
            };
            target=closed ? 1 : 0;
            for (guint i=0;i<G_N_ELEMENTS(pairs);i++)
                if (first == pairs[i].first && last == pairs[i].last && (modern || !pairs[i].modern_only)) target=1;
        }
        result=positions[target];
    }
    g_free(positions);
    return result;
}

void remove_all_tones(GArray *word){
    for(guint i=0;i<word->len;i++){
        gunichar c=g_array_index(word,gunichar,i);
        int tone=get_tone(c);
        if(tone!=TONE_NONE){
            CharInfo *info=get_info(c);
            gunichar nc;
            if(lookup_char(info->bare, info->diacritic, TONE_NONE, info->is_upper, &nc))
                g_array_index(word,gunichar,i)=nc;
        }
    }
}
gboolean apply_tone_at(GArray *word, int pos, int tone){
    if(pos<0||pos>=(int)word->len) return FALSE;
    gunichar c=g_array_index(word,gunichar,pos);
    CharInfo *info=get_info(c);
    if(!info||info->bare=='d') return FALSE;
    gunichar nc;
    if(!lookup_char(info->bare, info->diacritic, tone, info->is_upper, &nc)) return FALSE;
    g_array_index(word,gunichar,pos)=nc;
    return TRUE;
}
