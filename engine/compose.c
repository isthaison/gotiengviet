#include "internal.h"

/* Both input methods map keys to the same operations. There are no word lists,
 * English guesses, or method-specific mutation/undo algorithms here. */
typedef enum { OP_TONE, OP_SHAPE, OP_CLEAR, OP_SHORTCUT } Operation;
typedef struct {
    GtvMode mode;
    gunichar key;
    Operation operation;
    int mark;
    const char *targets;
    const char *expansion;
} KeyRule;
static const KeyRule key_rules[] = {
    {GTV_TELEX,'s',OP_TONE,TONE_SAC,NULL,NULL},
    {GTV_TELEX,'f',OP_TONE,TONE_HUYEN,NULL,NULL},
    {GTV_TELEX,'r',OP_TONE,TONE_HOI,NULL,NULL},
    {GTV_TELEX,'x',OP_TONE,TONE_NGA,NULL,NULL},
    {GTV_TELEX,'j',OP_TONE,TONE_NANG,NULL,NULL},
    {GTV_TELEX,'a',OP_SHAPE,DIAC_CIRCUMFLEX,"a",NULL},
    {GTV_TELEX,'e',OP_SHAPE,DIAC_CIRCUMFLEX,"e",NULL},
    {GTV_TELEX,'o',OP_SHAPE,DIAC_CIRCUMFLEX,"o",NULL},
    {GTV_TELEX,'d',OP_SHAPE,DIAC_STROKE,"d",NULL},
    {GTV_TELEX,'w',OP_SHAPE,DIAC_HORN,"uoa","ư"},
    {GTV_TELEX,'z',OP_CLEAR,0,NULL,NULL},
    {GTV_TELEX,'[',OP_SHORTCUT,0,NULL,"ươ"},
    {GTV_TELEX,']',OP_SHORTCUT,0,NULL,"ư"},
    {GTV_TELEX,'{',OP_SHORTCUT,0,NULL,"ƯƠ"},
    {GTV_TELEX,'}',OP_SHORTCUT,0,NULL,"Ư"},
    {GTV_VNI,'1',OP_TONE,TONE_SAC,NULL,NULL},
    {GTV_VNI,'2',OP_TONE,TONE_HUYEN,NULL,NULL},
    {GTV_VNI,'3',OP_TONE,TONE_HOI,NULL,NULL},
    {GTV_VNI,'4',OP_TONE,TONE_NGA,NULL,NULL},
    {GTV_VNI,'5',OP_TONE,TONE_NANG,NULL,NULL},
    {GTV_VNI,'6',OP_SHAPE,DIAC_CIRCUMFLEX,"aeo",NULL},
    {GTV_VNI,'7',OP_SHAPE,DIAC_HORN,"uo",NULL},
    {GTV_VNI,'8',OP_SHAPE,DIAC_BREVE,"a",NULL},
    {GTV_VNI,'9',OP_SHAPE,DIAC_STROKE,"d",NULL},
    {GTV_VNI,'0',OP_CLEAR,0,NULL,NULL},
};
static const KeyRule *key_rule(GtvMode mode, gunichar key) {
    for (guint i = 0; i < G_N_ELEMENTS(key_rules); i++)
        if (key_rules[i].mode == mode && key_rules[i].key == to_lower_g(key)) return &key_rules[i];
    return NULL;
}
static int current_tone(const GArray *buf) {
    for (guint i = 0; i < buf->len; i++) {
        int tone = get_tone(g_array_index(buf, gunichar, i));
        if (tone) return tone;
    }
    return TONE_NONE;
}
static int nucleus_start(const GArray *buf) {
    for (guint i = 0; i < buf->len; i++) {
        if (!is_vowel(g_array_index(buf, gunichar, i))) continue;
        /* u in qu is part of the onset when another vowel follows. */
        if (i == 1 && bare_lower(g_array_index(buf,gunichar,0)) == 'q' &&
            bare_lower(g_array_index(buf,gunichar,i)) == 'u' && i + 1 < buf->len &&
            is_vowel(g_array_index(buf,gunichar,i+1))) continue;
        return (int)i;
    }
    return -1;
}
static int nucleus_end(const GArray *buf) {
    for (int i = (int)buf->len - 1; i >= 0; i--)
        if (is_vowel(g_array_index(buf, gunichar, i))) return i;
    return -1;
}
/* Structural eligibility, not a dictionary or language detector. */
static gboolean syllable_eligible(const GArray *buf) {
    int first = nucleus_start(buf), last = nucleus_end(buf);
    if (first < 0) return is_valid_onset((gunichar *)buf->data, buf->len);
    if (!is_valid_onset((gunichar *)buf->data, first)) return FALSE;
    for (int i = first; i <= last; i++)
        if (!is_vowel(g_array_index(buf,gunichar,i))) return FALSE;
    int length = (int)buf->len - last - 1;
    if (length > 2) return FALSE;
    char coda[3] = {0};
    for (int i = 0; i < length; i++) {
        gunichar c = bare_lower(g_array_index(buf,gunichar,last+1+i));
        if (c > 127) return FALSE;
        coda[i] = (char)c;
    }
    return is_valid_coda(coda, length);
}
static void normalize_tone(GArray *buf, gboolean modern) {
    int tone = current_tone(buf);
    if (tone && syllable_eligible(buf)) {
        auto_promote_diphthong(buf);
        remove_all_tones(buf);
        apply_tone_at(buf, find_tone_position(buf, modern), tone);
    }
}
static gboolean shortcut(GArray *buf, const gchar *expansion, gunichar key) {
    glong length;
    gunichar *chars = g_utf8_to_ucs4(expansion, -1, NULL, &length, NULL);
    if (g_unichar_isupper(key)) for (glong i=0;i<length;i++) chars[i]=g_unichar_toupper(chars[i]);
    gboolean repeat = buf->len >= (guint)length &&
        !memcmp(&g_array_index(buf,gunichar,buf->len-length), chars, length*sizeof(gunichar));
    if (repeat) { g_array_set_size(buf, buf->len-length); g_array_append_val(buf,key); }
    else g_array_append_vals(buf,chars,length);
    g_free(chars);
    return TRUE;
}
static gboolean shape(GArray *buf, const KeyRule *rule, gunichar key, gboolean modern) {
    int first = nucleus_start(buf), last = nucleus_end(buf);
    int positions[2], count = 0;
    gboolean closed = last >= 0 && last < (int)buf->len - 1;
    /* uo is one shape target, including an existing uô with a tone. */
    if (rule->mark == DIAC_HORN) {
        for (int i = last; i > first; i--) {
            if (bare_lower(g_array_index(buf,gunichar,i-1)) == 'u' &&
                bare_lower(g_array_index(buf,gunichar,i)) == 'o') {
                positions[0]=i-1; positions[1]=i; count=2; break;
            }
        }
    }
    if (!count) {
        int best = -1;
        for (int i = (int)buf->len-1; i >= 0; i--) {
            gunichar bare = bare_lower(g_array_index(buf,gunichar,i));
            if (bare > 127 || !strchr(rule->targets, (char)bare)) continue;
            if (rule->mark != DIAC_STROKE && i < first) continue;
            /* For a mixed horn/breve key, closed nuclei prefer a; open nuclei
             * prefer u/o. A single target key uses exactly the same search. */
            int priority = rule->mark == DIAC_HORN && strchr(rule->targets,'a') ?
                ((bare == 'a') == closed ? 1 : 0) : 0;
            if (priority > best) { positions[0]=i; count=1; best=priority; }
        }
    }
    if (!count) {
        if (rule->expansion && first < 0) return shortcut(buf,rule->expansion,key);
        return FALSE;
    }
    gboolean repeat = TRUE;
    int marks[2];
    for (int i=0;i<count;i++) {
        gunichar c = g_array_index(buf,gunichar,positions[i]);
        marks[i] = rule->mark == DIAC_HORN && bare_lower(c) == 'a' ? DIAC_BREVE : rule->mark;
        repeat &= get_diac(c) == marks[i];
    }
    for (int i=0;i<count;i++) {
        gunichar c = g_array_index(buf,gunichar,positions[i]), changed;
        if (!lookup_char(bare_lower(c), repeat ? DIAC_NONE : marks[i], get_tone(c), g_unichar_isupper(c), &changed)) return FALSE;
        g_array_index(buf,gunichar,positions[i]) = changed;
    }
    /* The same undo rule for every shape: remove it and append one raw key. */
    if (repeat) g_array_append_val(buf,key);
    else normalize_tone(buf,modern);
    return TRUE;
}
gboolean gtv_apply_key(GArray *buf, gunichar key, GtvMode mode, gboolean modern) {
    gtv_init();
    const KeyRule *rule = key_rule(mode,key);
    if (!rule || !syllable_eligible(buf)) return FALSE;
    if (rule->operation == OP_SHORTCUT) return shortcut(buf,rule->expansion,key);
    if (rule->operation == OP_CLEAR) {
        if (!current_tone(buf)) return FALSE;
        remove_all_tones(buf);
        return TRUE;
    }
    if (rule->operation == OP_SHAPE) return shape(buf,rule,key,modern);
    if (nucleus_start(buf) < 0) return FALSE;
    gboolean repeat = current_tone(buf) == rule->mark;
    remove_all_tones(buf);
    if (repeat) g_array_append_val(buf,key);
    else {
        auto_promote_diphthong(buf);
        apply_tone_at(buf,find_tone_position(buf,modern),rule->mark);
    }
    return TRUE;
}
void gtv_compose(GArray *buf, gunichar key, GtvMode mode, gboolean modern) {
    if (!gtv_apply_key(buf,key,mode,modern)) {
        g_array_append_val(buf,key);
        normalize_tone(buf,modern);
    }
}
