#include "internal.h"

/* Global maps */
static GHashTable *char_info_map = NULL; // gunichar (as gpointer) -> CharInfo*
static GHashTable *reverse_map = NULL;   // key string "bare:dia:tone:upper" -> gunichar

gchar* reverse_key(gunichar bare, int dia, int tone, gboolean upper){
    return g_strdup_printf("%u:%d:%d:%d", (guint)bare, dia, tone, upper?1:0);
}

void register_char(gunichar ch, gunichar bare, int dia, int tone, gboolean upper){
    CharInfo *info = g_new(CharInfo,1);
    info->bare=bare; info->diacritic=dia; info->tone=tone; info->is_upper=upper;
    g_hash_table_insert(char_info_map, GUINT_TO_POINTER((guint)ch), info);
    gchar *k = reverse_key(bare,dia,tone,upper);
    g_hash_table_insert(reverse_map, k, GUINT_TO_POINTER((guint)ch));
}

void init_charset(void){
    if (char_info_map) return;
    char_info_map = g_hash_table_new(g_direct_hash, g_direct_equal);
    reverse_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    // lower groups - strings UTF-8
    struct { gunichar bare; int dia; const char *utf8; } lower[] = {
        {'a', DIAC_NONE, "aáàảãạ"},
        {'a', DIAC_BREVE, "ăắằẳẵặ"},
        {'a', DIAC_CIRCUMFLEX, "âấầẩẫậ"},
        {'e', DIAC_NONE, "eéèẻẽẹ"},
        {'e', DIAC_CIRCUMFLEX, "êếềểễệ"},
        {'i', DIAC_NONE, "iíìỉĩị"},
        {'o', DIAC_NONE, "oóòỏõọ"},
        {'o', DIAC_CIRCUMFLEX, "ôốồổỗộ"},
        {'o', DIAC_HORN, "ơớờởỡợ"},
        {'u', DIAC_NONE, "uúùủũụ"},
        {'u', DIAC_HORN, "ưứừửữự"},
        {'y', DIAC_NONE, "yýỳỷỹỵ"},
        {0,0,NULL}
    };
    for(int i=0; lower[i].utf8; i++){
        glong len;
        gunichar *ucs = g_utf8_to_ucs4(lower[i].utf8, -1, NULL, &len, NULL);
        for(int t=0; t<len; t++){
            register_char(ucs[t], lower[i].bare, lower[i].dia, t, FALSE);
        }
        g_free(ucs);
    }
    struct { gunichar bare; int dia; const char *utf8; } upper[] = {
        {'a', DIAC_NONE, "AÁÀẢÃẠ"},
        {'a', DIAC_BREVE, "ĂẮẰẲẴẶ"},
        {'a', DIAC_CIRCUMFLEX, "ÂẤẦẨẪẬ"},
        {'e', DIAC_NONE, "EÉÈẺẼẸ"},
        {'e', DIAC_CIRCUMFLEX, "ÊẾỀỂỄỆ"},
        {'i', DIAC_NONE, "IÍÌỈĨỊ"},
        {'o', DIAC_NONE, "OÓÒỎÕỌ"},
        {'o', DIAC_CIRCUMFLEX, "ÔỐỒỔỖỘ"},
        {'o', DIAC_HORN, "ƠỚỜỞỠỢ"},
        {'u', DIAC_NONE, "UÚÙỦŨỤ"},
        {'u', DIAC_HORN, "ƯỨỪỬỮỰ"},
        {'y', DIAC_NONE, "YÝỲỶỸỴ"},
        {0,0,NULL}
    };
    for(int i=0; upper[i].utf8; i++){
        glong len;
        gunichar *ucs = g_utf8_to_ucs4(upper[i].utf8, -1, NULL, &len, NULL);
        for(int t=0; t<len; t++){
            register_char(ucs[t], upper[i].bare, upper[i].dia, t, TRUE);
        }
        g_free(ucs);
    }
    register_char('d', 'd', DIAC_NONE, TONE_NONE, FALSE);
    register_char(0x0111, 'd', DIAC_STROKE, TONE_NONE, FALSE); // đ
    register_char('D', 'd', DIAC_NONE, TONE_NONE, TRUE);
    register_char(0x0110, 'd', DIAC_STROKE, TONE_NONE, TRUE); // Đ
}

CharInfo* get_info(gunichar c){
    return (CharInfo*)g_hash_table_lookup(char_info_map, GUINT_TO_POINTER((guint)c));
}
gboolean lookup_char(gunichar bare, int dia, int tone, gboolean upper, gunichar *out){
    gchar *k = reverse_key(bare,dia,tone,upper);
    gpointer v = g_hash_table_lookup(reverse_map, k);
    g_free(k);
    if (!v) return FALSE;
    *out = (gunichar)GPOINTER_TO_UINT(v);
    return TRUE;
}
gboolean is_vowel(gunichar c){
    CharInfo *info = get_info(c);
    if (!info) return FALSE;
    if (info->bare == 'd') return FALSE;
    return TRUE;
}
gboolean is_consonant(gunichar c){
    if (is_vowel(c)) return FALSE;
    return g_unichar_isalpha(c);
}
gunichar to_lower_g(gunichar c){ return g_unichar_tolower(c); }
gint get_tone(gunichar c){ CharInfo *i=get_info(c); return i?i->tone:TONE_NONE; }
gint get_diac(gunichar c){ CharInfo *i=get_info(c); return i?i->diacritic:DIAC_NONE; }
gunichar bare_lower(gunichar c){ CharInfo *i=get_info(c); if(i) return i->bare; return to_lower_g(c); }

gboolean toggle_breve(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info||info->bare!='a') return FALSE;
    int ndia=-1;
    if(info->diacritic==DIAC_BREVE) ndia=DIAC_NONE;
    else if(info->diacritic==DIAC_NONE || info->diacritic==DIAC_CIRCUMFLEX) ndia=DIAC_BREVE;
    else return FALSE;
    return lookup_char(info->bare, ndia, info->tone, info->is_upper, out);
}
gboolean toggle_circumflex(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info) return FALSE;
    int ndia=-1;
    if(info->bare=='a' || info->bare=='e' || info->bare=='o'){
        if(info->diacritic==DIAC_CIRCUMFLEX) ndia=DIAC_NONE;
        else if(info->diacritic==DIAC_NONE || info->diacritic==DIAC_BREVE) ndia=DIAC_CIRCUMFLEX;
        else return FALSE;
        return lookup_char(info->bare, ndia, info->tone, info->is_upper, out);
    }
    return FALSE;
}
gboolean toggle_horn(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info) return FALSE;
    if(info->bare!='o' && info->bare!='u') return FALSE;
    int ndia=-1;
    if(info->diacritic==DIAC_HORN) ndia=DIAC_NONE;
    else if(info->diacritic==DIAC_NONE || info->diacritic==DIAC_CIRCUMFLEX) ndia=DIAC_HORN;
    else return FALSE;
    return lookup_char(info->bare, ndia, info->tone, info->is_upper, out);
}
gboolean toggle_stroke(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info||info->bare!='d') return FALSE;
    int ndia = (info->diacritic==DIAC_STROKE)?DIAC_NONE:DIAC_STROKE;
    return lookup_char(info->bare, ndia, TONE_NONE, info->is_upper, out);
}

/* Phonology: find tone position */
