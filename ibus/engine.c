/* IBus engine - Telex/VNI thuần hệ thống, chỉ dùng libibus-1.0, glib-2.0
 * Build: gcc -O2 -o /usr/libexec/ibus-engine-gotiengviet ibus/engine.c $(pkg-config --cflags --libs ibus-1.0)
 * Không dùng lib ngoài, logic port từ Go engine (charset, phonology, telex)
 */
#include <ibus.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>

#define ENGINE_NAME "gotiengviet"

#define DIAC_NONE 0
#define DIAC_BREVE 1
#define DIAC_CIRCUMFLEX 2
#define DIAC_HORN 3
#define DIAC_STROKE 4

#define TONE_NONE 0
#define TONE_SAC 1
#define TONE_HUYEN 2
#define TONE_HOI 3
#define TONE_NGA 4
#define TONE_NANG 5

typedef struct {
    gunichar bare; // 'a','e','i','o','u','y','d'
    int diacritic;
    int tone;
    gboolean is_upper;
} CharInfo;

/* Global maps */
static GHashTable *char_info_map = NULL; // gunichar (as gpointer) -> CharInfo*
static GHashTable *reverse_map = NULL;   // key string "bare:dia:tone:upper" -> gunichar

static guint charinfo_hash(gconstpointer k){
    const CharInfo *c = k;
    return (guint)(c->bare*31 + c->diacritic*17 + c->tone*7 + (c->is_upper?1:0));
}
static gboolean charinfo_equal(gconstpointer a, gconstpointer b){
    const CharInfo *x=a, *y=b;
    return x->bare==y->bare && x->diacritic==y->diacritic && x->tone==y->tone && x->is_upper==y->is_upper;
}

static gchar* reverse_key(gunichar bare, int dia, int tone, gboolean upper){
    return g_strdup_printf("%u:%d:%d:%d", (guint)bare, dia, tone, upper?1:0);
}

static void register_char(gunichar ch, gunichar bare, int dia, int tone, gboolean upper){
    CharInfo *info = g_new(CharInfo,1);
    info->bare=bare; info->diacritic=dia; info->tone=tone; info->is_upper=upper;
    g_hash_table_insert(char_info_map, GUINT_TO_POINTER((guint)ch), info);
    gchar *k = reverse_key(bare,dia,tone,upper);
    g_hash_table_insert(reverse_map, k, GUINT_TO_POINTER((guint)ch));
}

static void init_charset(void){
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

static CharInfo* get_info(gunichar c){
    return (CharInfo*)g_hash_table_lookup(char_info_map, GUINT_TO_POINTER((guint)c));
}
static gboolean lookup_char(gunichar bare, int dia, int tone, gboolean upper, gunichar *out){
    gchar *k = reverse_key(bare,dia,tone,upper);
    gpointer v = g_hash_table_lookup(reverse_map, k);
    g_free(k);
    if (!v) return FALSE;
    *out = (gunichar)GPOINTER_TO_UINT(v);
    return TRUE;
}
static gboolean is_vowel(gunichar c){
    CharInfo *info = get_info(c);
    if (!info) return FALSE;
    if (info->bare == 'd') return FALSE;
    return TRUE;
}
static gboolean is_consonant(gunichar c){
    if (is_vowel(c)) return FALSE;
    return g_unichar_isalpha(c);
}
static gunichar to_lower_g(gunichar c){ return g_unichar_tolower(c); }
static gint get_tone(gunichar c){ CharInfo *i=get_info(c); return i?i->tone:TONE_NONE; }
static gint get_diac(gunichar c){ CharInfo *i=get_info(c); return i?i->diacritic:DIAC_NONE; }
static gunichar bare_lower(gunichar c){ CharInfo *i=get_info(c); if(i) return i->bare; return to_lower_g(c); }

static gboolean toggle_breve(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info||info->bare!='a') return FALSE;
    int ndia=-1;
    if(info->diacritic==DIAC_BREVE) ndia=DIAC_NONE;
    else if(info->diacritic==DIAC_NONE) ndia=DIAC_BREVE;
    else return FALSE;
    return lookup_char(info->bare, ndia, info->tone, info->is_upper, out);
}
static gboolean toggle_circumflex(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info) return FALSE;
    int ndia=-1;
    if(info->bare=='a' || info->bare=='e' || info->bare=='o'){
        if(info->diacritic==DIAC_CIRCUMFLEX) ndia=DIAC_NONE;
        else if(info->diacritic==DIAC_NONE) ndia=DIAC_CIRCUMFLEX;
        else return FALSE;
        return lookup_char(info->bare, ndia, info->tone, info->is_upper, out);
    }
    return FALSE;
}
static gboolean toggle_horn(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info) return FALSE;
    if(info->bare!='o' && info->bare!='u') return FALSE;
    int ndia=-1;
    if(info->diacritic==DIAC_HORN) ndia=DIAC_NONE;
    else if(info->diacritic==DIAC_NONE) ndia=DIAC_HORN;
    else return FALSE;
    return lookup_char(info->bare, ndia, info->tone, info->is_upper, out);
}
static gboolean toggle_stroke(gunichar in, gunichar *out){
    CharInfo *info=get_info(in); if(!info||info->bare!='d') return FALSE;
    int ndia = (info->diacritic==DIAC_STROKE)?DIAC_NONE:DIAC_STROKE;
    return lookup_char(info->bare, ndia, TONE_NONE, info->is_upper, out);
}

/* Phonology: find tone position */
static int find_tone_position(GArray *word, gboolean modern){
    int n = word->len;
    if(n==0) return -1;
    GArray *vidx = g_array_new(FALSE,FALSE,sizeof(int));
    for(int i=0;i<n;i++){
        gunichar c = g_array_index(word, gunichar, i);
        if(is_vowel(c)) g_array_append_val(vidx, i);
    }
    if(vidx->len==0){ g_array_free(vidx,TRUE); return -1; }
    if(vidx->len==1){ int r=g_array_index(vidx,int,0); g_array_free(vidx,TRUE); return r; }
    // hasQu/hasGi
    gunichar *arr = (gunichar*)word->data;
    gboolean hasQu=FALSE, hasGi=FALSE;
    if(n>=2 && to_lower_g(arr[0])=='q' && to_lower_g(arr[1])=='u') hasQu=TRUE;
    if(n>=2 && to_lower_g(arr[0])=='g' && to_lower_g(arr[1])=='i') hasGi=TRUE;
    if(hasQu){
        for(guint i=0;i<vidx->len;i++){
            int vi=g_array_index(vidx,int,i);
            if(vi==1 && bare_lower(g_array_index(word,gunichar,vi))=='u'){
                g_array_remove_index(vidx,i); break;
            }
        }
        if(vidx->len==0){g_array_free(vidx,TRUE);return -1;}
        if(vidx->len==1){int r=g_array_index(vidx,int,0); g_array_free(vidx,TRUE); return r;}
    }
    if(hasGi && vidx->len>1){
        for(guint i=0;i<vidx->len;i++){
            int vi=g_array_index(vidx,int,i);
            if(vi==1 && bare_lower(g_array_index(word,gunichar,vi))=='i'){
                if(vidx->len>=2){ g_array_remove_index(vidx,i); break; }
            }
        }
        if(vidx->len==0){g_array_free(vidx,TRUE);return -1;}
        if(vidx->len==1){int r=g_array_index(vidx,int,0); g_array_free(vidx,TRUE); return r;}
    }
    // priority diacritic
    GArray *withDia = g_array_new(FALSE,FALSE,sizeof(int));
    for(guint i=0;i<vidx->len;i++){
        int vi=g_array_index(vidx,int,i);
        int dia=get_diac(g_array_index(word,gunichar,vi));
        if(dia==DIAC_CIRCUMFLEX||dia==DIAC_HORN||dia==DIAC_BREVE) g_array_append_val(withDia, vi);
    }
    if(withDia->len==1){ int r=g_array_index(withDia,int,0); g_array_free(vidx,TRUE); g_array_free(withDia,TRUE); return r; }
    if(withDia->len>1){
        // prefer circumflex
        for(guint i=0;i<withDia->len;i++){
            int vi=g_array_index(withDia,int,i);
            if(get_diac(g_array_index(word,gunichar,vi))==DIAC_CIRCUMFLEX){ g_array_free(vidx,TRUE); g_array_free(withDia,TRUE); return vi; }
        }
        for(guint i=0;i<withDia->len;i++){
            int vi=g_array_index(withDia,int,i);
            CharInfo *ci=get_info(g_array_index(word,gunichar,vi));
            if(ci && ci->bare=='o' && ci->diacritic==DIAC_HORN){ g_array_free(vidx,TRUE); g_array_free(withDia,TRUE); return vi; }
        }
        int r=g_array_index(withDia,int,withDia->len-1); g_array_free(vidx,TRUE); g_array_free(withDia,TRUE); return r;
    }
    g_array_free(withDia,TRUE);
    // special pairs
    // build vowel string bare lower
    int first=g_array_index(vidx,int,0);
    int last=g_array_index(vidx,int,vidx->len-1);
    gboolean contiguous = (last-first+1 == (int)vidx->len);
    GString *vstr=g_string_new("");
    if(contiguous){
        for(int i=first;i<=last;i++) g_string_append_unichar(vstr, to_lower_g(bare_lower(g_array_index(word,gunichar,i))));
    } else {
        for(guint i=0;i<vidx->len;i++) g_string_append_unichar(vstr, to_lower_g(bare_lower(g_array_index(word,gunichar,g_array_index(vidx,int,i)))));
    }
    const char *pairs_modern[]={"oa","oe","uy",NULL};
    const char *pairs_other[]={"oo","uo","ie","ua",NULL};
    for(int p=0; pairs_modern[p]; p++){
        if(strstr(vstr->str, pairs_modern[p])){
            int r;
            if(modern){
                if(vidx->len==2) r=g_array_index(vidx,int,1);
                else if(vidx->len==3) r=g_array_index(vidx,int,1);
                else r=g_array_index(vidx,int,1);
            } else {
                if(vidx->len==2) r=g_array_index(vidx,int,0);
                else if(vidx->len==3) r=g_array_index(vidx,int,1);
                else r=g_array_index(vidx,int,0);
            }
            g_string_free(vstr,TRUE); g_array_free(vidx,TRUE); return r;
        }
    }
    for(int p=0; pairs_other[p]; p++){
        if(strstr(vstr->str, pairs_other[p])){
            int r;
            if(vidx->len==2) r=g_array_index(vidx,int,1);
            else if(vidx->len==3) r=g_array_index(vidx,int,1);
            else r=g_array_index(vidx,int,1);
            g_string_free(vstr,TRUE); g_array_free(vidx,TRUE); return r;
        }
    }
    g_string_free(vstr,TRUE);
    // hasFinal
    gboolean hasFinal=FALSE;
    if(n>0){
        gunichar lc = g_array_index(word,gunichar,n-1);
        if(!is_vowel(lc) && g_unichar_isalpha(lc) && last < n-1) hasFinal=TRUE;
        else if(is_vowel(lc)) hasFinal=FALSE;
    }
    if(vidx->len==2){
        int r = hasFinal ? g_array_index(vidx,int,1) : g_array_index(vidx,int,0);
        g_array_free(vidx,TRUE); return r;
    }
    if(vidx->len==3){ int r=g_array_index(vidx,int,1); g_array_free(vidx,TRUE); return r; }
    int r=g_array_index(vidx,int,1); g_array_free(vidx,TRUE); return r;
}

static void remove_all_tones(GArray *word){
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
static gboolean apply_tone_at(GArray *word, int pos, int tone){
    if(pos<0||pos>=(int)word->len) return FALSE;
    gunichar c=g_array_index(word,gunichar,pos);
    CharInfo *info=get_info(c);
    if(!info||info->bare=='d') return FALSE;
    gunichar nc;
    if(!lookup_char(info->bare, info->diacritic, tone, info->is_upper, &nc)) return FALSE;
    g_array_index(word,gunichar,pos)=nc;
    return TRUE;
}
static gboolean apply_mark(GArray *buf, int tone, gboolean modern){
    if(tone==TONE_NONE) return FALSE;
    int pos=find_tone_position(buf, modern);
    if(pos==-1) return FALSE;
    int cur=get_tone(g_array_index(buf,gunichar,pos));
    if(cur==tone) return TRUE;
    // clear
    GArray *tmp=g_array_sized_new(FALSE,FALSE,sizeof(gunichar),buf->len);
    g_array_append_vals(tmp, buf->data, buf->len);
    remove_all_tones(tmp);
    int newpos=find_tone_position(tmp, modern);
    if(newpos==-1){ g_array_free(tmp,TRUE); return FALSE; }
    apply_tone_at(tmp,newpos,tone);
    // copy back
    g_array_set_size(buf,0);
    g_array_append_vals(buf, tmp->data, tmp->len);
    g_array_free(tmp,TRUE);
    return TRUE;
}
static gboolean try_remove(GArray *buf){
    if(buf->len==0) return FALSE;
    gboolean hasTone=FALSE;
    for(guint i=0;i<buf->len;i++) if(get_tone(g_array_index(buf,gunichar,i))!=TONE_NONE){hasTone=TRUE;break;}
    if(hasTone){
        remove_all_tones(buf); return TRUE;
    }
    for(int i=(int)buf->len-1;i>=0;i--){
        gunichar c=g_array_index(buf,gunichar,i);
        if(!is_vowel(c)) continue;
        int dia=get_diac(c);
        if(dia==DIAC_NONE||dia==DIAC_STROKE) continue;
        gunichar nc; gboolean ok=FALSE;
        if(dia==DIAC_BREVE) ok=toggle_breve(c,&nc);
        else if(dia==DIAC_CIRCUMFLEX) ok=toggle_circumflex(c,&nc);
        else if(dia==DIAC_HORN) ok=toggle_horn(c,&nc);
        if(ok){ g_array_index(buf,gunichar,i)=nc; return TRUE; }
    }
    return FALSE;
}

/* Telex helpers */
static gboolean telex_is_tone(gunichar k, int *out){
    switch(to_lower_g(k)){
        case 's': *out=TONE_SAC; return TRUE;
        case 'f': *out=TONE_HUYEN; return TRUE;
        case 'r': *out=TONE_HOI; return TRUE;
        case 'x': *out=TONE_NGA; return TRUE;
        case 'j': *out=TONE_NANG; return TRUE;
        default: return FALSE;
    }
}
static gboolean telex_transform(GArray *buf, gunichar key, gboolean modern){
    if(buf->len>0 && g_array_index(buf,gunichar,0)==':'){
        gboolean hasClosing=FALSE;
        for(guint i=1;i<buf->len;i++) if(g_array_index(buf,gunichar,i)==':'){hasClosing=TRUE;break;}
        if(!hasClosing) return FALSE;
    }
    if(buf->len==0 && key=='w'){ gunichar c=0x01B0; g_array_append_val(buf,c); return TRUE; }
    if(buf->len==0 && key=='W'){ gunichar c=0x01AF; g_array_append_val(buf,c); return TRUE; }
    if(buf->len==0 && (key=='w'||key=='W')){
        gunichar w = (key=='w')? 0x01B0 : 0x01AF;
        g_array_set_size(buf,0); g_array_append_val(buf,w); return TRUE;
    }
    if(to_lower_g(key)=='d' && buf->len>0){
        gunichar last=g_array_index(buf,gunichar,buf->len-1);
        if(bare_lower(last)=='d'){
            gunichar nc; if(toggle_stroke(last,&nc)){ g_array_index(buf,gunichar,buf->len-1)=nc; return TRUE; }
        }
    }
    gunichar lk=to_lower_g(key);
    if(lk=='a' && buf->len>0){
        gunichar last=g_array_index(buf,gunichar,buf->len-1);
        if(bare_lower(last)=='a' && get_diac(last)==DIAC_NONE){
            gunichar nc; if(toggle_circumflex(last,&nc)){ g_array_index(buf,gunichar,buf->len-1)=nc; return TRUE; }
        } else if(bare_lower(last)=='a' && get_diac(last)==DIAC_CIRCUMFLEX){
            gunichar nc; if(toggle_circumflex(last,&nc)){ g_array_index(buf,gunichar,buf->len-1)=nc; return TRUE; }
        }
    }
    if(lk=='e' && buf->len>0){
        gunichar last=g_array_index(buf,gunichar,buf->len-1);
        if(bare_lower(last)=='e'){
            int dia=get_diac(last);
            if(dia==DIAC_NONE||dia==DIAC_CIRCUMFLEX){
                gunichar nc; if(toggle_circumflex(last,&nc)){ g_array_index(buf,gunichar,buf->len-1)=nc; return TRUE; }
            }
        }
    }
    if(lk=='o' && buf->len>0){
        gunichar last=g_array_index(buf,gunichar,buf->len-1);
        if(bare_lower(last)=='o'){
            int dia=get_diac(last);
            if(dia==DIAC_NONE||dia==DIAC_CIRCUMFLEX){
                gunichar nc; if(toggle_circumflex(last,&nc)){ g_array_index(buf,gunichar,buf->len-1)=nc; return TRUE; }
            }
        }
    }
    // uow -> ươ shortcut before single w
    if(lk=='w' && buf->len>=2){
        gunichar sl=g_array_index(buf,gunichar,buf->len-2);
        gunichar last=g_array_index(buf,gunichar,buf->len-1);
        if(bare_lower(sl)=='u' && bare_lower(last)=='o' && get_diac(sl)==DIAC_NONE && get_diac(last)==DIAC_NONE){
            gunichar nc1,nc2;
            if(toggle_horn(sl,&nc1) && toggle_horn(last,&nc2)){
                g_array_index(buf,gunichar,buf->len-2)=nc1;
                g_array_index(buf,gunichar,buf->len-1)=nc2;
                return TRUE;
            }
        }
    }
    if(lk=='w' && buf->len>0){
        gboolean hasFinal=FALSE;
        if(buf->len>0){
            gunichar last=g_array_index(buf,gunichar,buf->len-1);
            if(is_consonant(last)){
                for(int i=(int)buf->len-1;i>=0;i--){
                    if(is_vowel(g_array_index(buf,gunichar,i))){
                        if(i < (int)buf->len-1) hasFinal=TRUE;
                        break;
                    }
                }
            }
        }
        if(hasFinal){
            // Có phụ âm cuối: ưu tiên a (hoac + w -> hoăc)
            for(int i=(int)buf->len-1;i>=0;i--){
                gunichar c=g_array_index(buf,gunichar,i);
                if(bare_lower(c)=='a'){
                    int dia=get_diac(c);
                    if(dia==DIAC_NONE||dia==DIAC_BREVE){
                        if(dia==DIAC_CIRCUMFLEX) continue;
                        gunichar nc; if(toggle_breve(c,&nc)){ g_array_index(buf,gunichar,i)=nc; return TRUE; }
                    }
                }
            }
            for(int i=(int)buf->len-1;i>=0;i--){
                gunichar c=g_array_index(buf,gunichar,i);
                gunichar bare=bare_lower(c);
                if(bare=='u' || bare=='o'){
                    int dia=get_diac(c);
                    if(dia==DIAC_NONE){
                        gunichar nc; if(toggle_horn(c,&nc)){ g_array_index(buf,gunichar,i)=nc; return TRUE; }
                    } else if(dia==DIAC_HORN){
                    // Nguyên tắc gõ lại để xóa: ư + w -> u
                    gunichar nc; if(toggle_horn(c,&nc)){ g_array_index(buf,gunichar,i)=nc; return TRUE; }
                }
                }
            }
        } else {
            // Không có phụ âm cuối: ưu tiên u/o (thuaw -> thưa)
            for(int i=(int)buf->len-1;i>=0;i--){
                gunichar c=g_array_index(buf,gunichar,i);
                gunichar bare=bare_lower(c);
                if(bare=='u' || bare=='o'){
                    int dia=get_diac(c);
                    if(dia==DIAC_NONE){
                        gunichar nc; if(toggle_horn(c,&nc)){ g_array_index(buf,gunichar,i)=nc; return TRUE; }
                    } else if(dia==DIAC_HORN){
                    // Nguyên tắc gõ lại để xóa: ư + w -> u
                    gunichar nc; if(toggle_horn(c,&nc)){ g_array_index(buf,gunichar,i)=nc; return TRUE; }
                }
                }
            }
            for(int i=(int)buf->len-1;i>=0;i--){
                gunichar c=g_array_index(buf,gunichar,i);
                if(bare_lower(c)=='a'){
                    int dia=get_diac(c);
                    if(dia==DIAC_NONE||dia==DIAC_BREVE){
                        if(dia==DIAC_CIRCUMFLEX) continue;
                        gunichar nc; if(toggle_breve(c,&nc)){ g_array_index(buf,gunichar,i)=nc; return TRUE; }
                    }
                }
            }
        }
        // w literal + w => ư
        for(int i=(int)buf->len-1;i>=0;i--){
            gunichar c=g_array_index(buf,gunichar,i);
            if(c=='w' || c=='W'){
                gunichar uw = g_unichar_isupper(c) ? 0x01AF : 0x01B0;
                g_array_index(buf,gunichar,i)=uw;
                return TRUE;
            }
        }
    }
    int tone;
    if(telex_is_tone(key,&tone)){
        gboolean hasV=FALSE;
        for(guint i=0;i<buf->len;i++) if(is_vowel(g_array_index(buf,gunichar,i))){hasV=TRUE;break;}
        if(hasV){
            int pos=find_tone_position(buf, modern);
            if(pos!=-1 && get_tone(g_array_index(buf,gunichar,pos))==tone){
                // Cùng dấu đã có -> gõ s lần 2 để ra s thường (xóa dấu và thêm s)
                GArray *copy=g_array_sized_new(FALSE,FALSE,sizeof(gunichar),buf->len);
                g_array_append_vals(copy, buf->data, buf->len);
                gunichar c=g_array_index(copy,gunichar,pos);
                CharInfo *info=get_info(c);
                if(info){
                    gunichar nc;
                    if(lookup_char(info->bare, info->diacritic, TONE_NONE, info->is_upper, &nc)){
                        g_array_index(copy,gunichar,pos)=nc;
                    } else {
                        remove_all_tones(copy);
                    }
                } else {
                    remove_all_tones(copy);
                }
                g_array_append_val(copy, key);
                g_array_set_size(buf,0);
                g_array_append_vals(buf, copy->data, copy->len);
                g_array_free(copy,TRUE);
                return TRUE;
            }
            GArray *copy=g_array_sized_new(FALSE,FALSE,sizeof(gunichar),buf->len);
            g_array_append_vals(copy, buf->data, buf->len);
            gboolean ok=apply_mark(copy,tone,modern);
            if(ok){
                g_array_set_size(buf,0);
                g_array_append_vals(buf, copy->data, copy->len);
                g_array_free(copy,TRUE);
                return TRUE;
            }
            g_array_free(copy,TRUE);
        }
    }
    if(lk=='z'){
        GArray *copy=g_array_sized_new(FALSE,FALSE,sizeof(gunichar),buf->len);
        g_array_append_vals(copy, buf->data, buf->len);
        if(try_remove(copy)){
            g_array_set_size(buf,0);
            g_array_append_vals(buf, copy->data, copy->len);
            g_array_free(copy,TRUE);
            return TRUE;
        }
        g_array_free(copy,TRUE);
    }
    if(lk=='w'){
        gboolean should=FALSE;
        if(buf->len==0) should=TRUE;
        else {
            gunichar last=g_array_index(buf,gunichar,buf->len-1);
            if(is_consonant(last) && bare_lower(last)!='d') should=TRUE;
        }
        if(should){
            gunichar w = (key=='W')?0x01AF:0x01B0;
            g_array_append_val(buf,w);
            return TRUE;
        }
    }
    return FALSE;
}

/* Helpers to convert GString <-> GArray ucs4 */
static GArray* gstring_to_ucs4(GString *s){
    glong len;
    gunichar *ucs = g_utf8_to_ucs4(s->str, -1, NULL, &len, NULL);
    GArray *arr = g_array_sized_new(FALSE,FALSE,sizeof(gunichar),len);
    if(ucs){
        g_array_append_vals(arr, ucs, len);
        g_free(ucs);
    }
    return arr;
}
static void ucs4_to_gstring(GArray *arr, GString *s){
    gchar *utf8 = g_ucs4_to_utf8((gunichar*)arr->data, arr->len, NULL, NULL, NULL);
    if(utf8){
        g_string_assign(s, utf8);
        g_free(utf8);
    } else {
        g_string_assign(s,"");
    }
}

/* IBus Engine definition */
typedef struct _GoTiengVietEngine IBusGoTiengVietEngine;
typedef struct _GoTiengVietEngineClass IBusGoTiengVietEngineClass;
struct _GoTiengVietEngine { IBusEngine parent; GString *preedit; gboolean mode_telex; gboolean modern; gboolean spellcheck; };
struct _GoTiengVietEngineClass { IBusEngineClass parent; };
G_DEFINE_TYPE(IBusGoTiengVietEngine, ibus_gotiengviet_engine, IBUS_TYPE_ENGINE)

static void ibus_gotiengviet_engine_reset(IBusGoTiengVietEngine *e){
    if(e->preedit) g_string_assign(e->preedit,"");
}
static gboolean ibus_gotiengviet_engine_process_key_event(IBusEngine *engine, guint keyval, guint keycode, guint modifiers){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    if(modifiers & IBUS_RELEASE_MASK) return FALSE;
    // Backspace
    if(keyval==IBUS_BackSpace){
        if(e->preedit->len>0){
            // remove last utf8 char
            gchar *prev = g_utf8_prev_char(e->preedit->str + e->preedit->len);
            g_string_truncate(e->preedit, prev - e->preedit->str);
            IBusText *t=ibus_text_new_from_string(e->preedit->str);
            ibus_engine_update_preedit_text(engine,t,e->preedit->len,TRUE);
            return TRUE;
        }
        return FALSE;
    }
    if(keyval==IBUS_space){
        if(e->preedit->len>0){
            gchar *commit = g_strdup(e->preedit->str);
            gchar *with_space = g_strdup_printf("%s ",commit);
            IBusText *t=ibus_text_new_from_string(with_space);
            ibus_engine_commit_text(engine,t);
            g_free(commit); g_free(with_space);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            return TRUE;
        }
        return FALSE;
    }
    // punctuation commit
    if((keyval>=IBUS_exclam && keyval<=IBUS_slash) || (keyval>=IBUS_colon && keyval<=IBUS_at) || (keyval>=IBUS_bracketleft && keyval<=IBUS_grave) || (keyval>=IBUS_braceleft && keyval<=IBUS_asciitilde)){
        // if preedit has content, commit it + punctuation
        if(e->preedit->len>0 && (keyval==IBUS_comma || keyval==IBUS_period || keyval==IBUS_question || keyval==IBUS_exclam || keyval==IBUS_colon || keyval==IBUS_semicolon)){
            gchar c=(gchar)keyval;
            gchar *commit=g_strdup_printf("%s%c",e->preedit->str,c);
            IBusText *t=ibus_text_new_from_string(commit);
            ibus_engine_commit_text(engine,t);
            g_free(commit);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            return TRUE;
        }
    }
    if(keyval>=IBUS_a && keyval<=IBUS_z){
        gchar c=(gchar)keyval;
        // try telex transform
        GArray *buf=gstring_to_ucs4(e->preedit);
        gunichar gc=(gunichar)c;
        gboolean consumed=FALSE;
        if(e->mode_telex) consumed=telex_transform(buf, gc, e->modern);
        else {
            // VNI: digits already, but here key is letter, just append
            consumed=FALSE;
        }
        if(consumed){
            ucs4_to_gstring(buf, e->preedit);
            IBusText *t=ibus_text_new_from_string(e->preedit->str);
            ibus_engine_update_preedit_text(engine,t,e->preedit->len,TRUE);
            g_array_free(buf,TRUE);
            return TRUE;
        }
        g_array_free(buf,TRUE);
        // not consumed -> append
        g_string_append_c(e->preedit,c);
        IBusText *t=ibus_text_new_from_string(e->preedit->str);
        ibus_engine_update_preedit_text(engine,t,e->preedit->len,TRUE);
        return TRUE;
    }
    if(keyval>=IBUS_A && keyval<=IBUS_Z){
        gchar c=(gchar)keyval;
        GArray *buf=gstring_to_ucs4(e->preedit);
        gunichar gc=(gunichar)c;
        gboolean consumed=FALSE;
        if(e->mode_telex) consumed=telex_transform(buf, gc, e->modern);
        if(consumed){
            ucs4_to_gstring(buf, e->preedit);
            IBusText *t=ibus_text_new_from_string(e->preedit->str);
            ibus_engine_update_preedit_text(engine,t,e->preedit->len,TRUE);
            g_array_free(buf,TRUE);
            return TRUE;
        }
        g_array_free(buf,TRUE);
        g_string_append_c(e->preedit,c);
        IBusText *t=ibus_text_new_from_string(e->preedit->str);
        ibus_engine_update_preedit_text(engine,t,e->preedit->len,TRUE);
        return TRUE;
    }
    // digits for VNI mode or letter d?
    if(keyval>=IBUS_0 && keyval<=IBUS_9){
        gchar c=(gchar)keyval;
        if(!e->mode_telex){
            GArray *buf=gstring_to_ucs4(e->preedit);
            // VNI handling simplified: 6,7,8,9,1-5,0
            gboolean consumed=FALSE;
            // inline VNI transform
            if(c=='6'){
                // toggle circumflex last
                for(int i=(int)buf->len-1;i>=0;i--){
                    gunichar ch=g_array_index(buf,gunichar,i);
                    if(bare_lower(ch)=='a'||bare_lower(ch)=='e'||bare_lower(ch)=='o'){
                        gunichar nc; if(toggle_circumflex(ch,&nc)){g_array_index(buf,gunichar,i)=nc; consumed=TRUE; break;}
                    }
                }
            } else if(c=='7'){
                for(int i=(int)buf->len-1;i>=0;i--){
                    gunichar ch=g_array_index(buf,gunichar,i);
                    if(bare_lower(ch)=='o'||bare_lower(ch)=='u'){
                        gunichar nc; if(toggle_horn(ch,&nc)){g_array_index(buf,gunichar,i)=nc; consumed=TRUE; break;}
                    }
                }
            } else if(c=='8'){
                for(int i=(int)buf->len-1;i>=0;i--){
                    gunichar ch=g_array_index(buf,gunichar,i);
                    if(bare_lower(ch)=='a'){
                        gunichar nc; if(toggle_breve(ch,&nc)){g_array_index(buf,gunichar,i)=nc; consumed=TRUE; break;}
                    }
                }
            } else if(c=='9'){
                for(int i=(int)buf->len-1;i>=0;i--){
                    gunichar ch=g_array_index(buf,gunichar,i);
                    if(bare_lower(ch)=='d'){ gunichar nc; if(toggle_stroke(ch,&nc)){g_array_index(buf,gunichar,i)=nc; consumed=TRUE; break;}}
                }
            } else if(c>='1' && c<='5'){
                int tone=c-'0';
                gboolean hasV=FALSE; for(guint i=0;i<buf->len;i++) if(is_vowel(g_array_index(buf,gunichar,i))){hasV=TRUE;break;}
                if(hasV){
                    int pos=find_tone_position(buf, e->modern);
                    if(pos!=-1 && get_tone(g_array_index(buf,gunichar,pos))==tone){
                        // Nguyên tắc gõ lại để xóa: a1->á, á1->a
                        GArray *copy=g_array_sized_new(FALSE,FALSE,sizeof(gunichar),buf->len);
                        g_array_append_vals(copy, buf->data, buf->len);
                        gunichar c2=g_array_index(copy,gunichar,pos);
                        CharInfo *info2=get_info(c2);
                        if(info2){
                            gunichar nc;
                            if(lookup_char(info2->bare, info2->diacritic, TONE_NONE, info2->is_upper, &nc)){
                                g_array_index(copy,gunichar,pos)=nc;
                            } else {
                                remove_all_tones(copy);
                            }
                        } else {
                            remove_all_tones(copy);
                        }
                        g_array_set_size(buf,0);
                        g_array_append_vals(buf,copy->data,copy->len);
                        g_array_free(copy,TRUE);
                        consumed=TRUE;
                    } else {
                        GArray *copy=g_array_sized_new(FALSE,FALSE,sizeof(gunichar),buf->len);
                        g_array_append_vals(copy, buf->data, buf->len);
                        if(apply_mark(copy,tone,e->modern)){ consumed=TRUE; g_array_set_size(buf,0); g_array_append_vals(buf,copy->data,copy->len); }
                        g_array_free(copy,TRUE);
                    }
                }
            } else if(c=='0'){
                GArray *copy=g_array_sized_new(FALSE,FALSE,sizeof(gunichar),buf->len);
                g_array_append_vals(copy, buf->data, buf->len);
                if(try_remove(copy)){ consumed=TRUE; g_array_set_size(buf,0); g_array_append_vals(buf,copy->data,copy->len); }
                g_array_free(copy,TRUE);
            }
            if(consumed){
                ucs4_to_gstring(buf, e->preedit);
                IBusText *t=ibus_text_new_from_string(e->preedit->str);
                ibus_engine_update_preedit_text(engine,t,e->preedit->len,TRUE);
                g_array_free(buf,TRUE);
                return TRUE;
            }
            g_array_free(buf,TRUE);
        }
        // if not consumed, append digit (or commit?)
        g_string_append_c(e->preedit,c);
        IBusText *t=ibus_text_new_from_string(e->preedit->str);
        ibus_engine_update_preedit_text(engine,t,e->preedit->len,TRUE);
        return TRUE;
    }
    // Enter -> commit
    if(keyval==IBUS_Return || keyval==IBUS_KP_Enter){
        if(e->preedit->len>0){
            IBusText *t=ibus_text_new_from_string(e->preedit->str);
            ibus_engine_commit_text(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            return TRUE;
        }
        return FALSE;
    }
    // Other keys: commit preedit and forward
    if(e->preedit->len>0){
        IBusText *t=ibus_text_new_from_string(e->preedit->str);
        ibus_engine_commit_text(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
    }
    return FALSE;
}
static gboolean load_config(gboolean *is_telex, gboolean *modern, gboolean *spell){
    gchar *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    gboolean telex = TRUE;
    gboolean mod = TRUE;
    gboolean spellcheck = TRUE;
    GKeyFile *kf = g_key_file_new();
    if(g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)){
        gchar *method = g_key_file_get_string(kf, "input", "method", NULL);
        if(method){
            if(g_strcmp0(method, "vni")==0 || g_strcmp0(method, "VNI")==0) telex=FALSE;
            else telex=TRUE;
            g_free(method);
        }
        gchar *mod_str = g_key_file_get_string(kf, "input", "modern", NULL);
        if(mod_str){
            if(g_strcmp0(mod_str, "false")==0 || g_strcmp0(mod_str, "False")==0 || g_strcmp0(mod_str, "0")==0) mod=FALSE;
            else mod=TRUE;
            g_free(mod_str);
        }
        gchar *spell_str = g_key_file_get_string(kf, "input", "spellcheck", NULL);
        if(spell_str){
            if(g_strcmp0(spell_str, "false")==0 || g_strcmp0(spell_str, "False")==0 || g_strcmp0(spell_str, "0")==0) spellcheck=FALSE;
            else spellcheck=TRUE;
            g_free(spell_str);
        }
        g_key_file_free(kf);
    }
    g_free(path);
    if(is_telex) *is_telex = telex;
    if(modern) *modern = mod;
    if(spell) *spell = spellcheck;
    return TRUE;
}
static void ibus_gotiengviet_engine_focus_in(IBusEngine *engine){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    gboolean telex, modern;
    load_config(&telex, &modern, NULL);
    e->mode_telex=telex;
    e->modern=modern;
}
static void ibus_gotiengviet_engine_class_init(IBusGoTiengVietEngineClass *klass){
    IBusEngineClass *ec=IBUS_ENGINE_CLASS(klass);
    ec->process_key_event=ibus_gotiengviet_engine_process_key_event;
    ec->focus_in=ibus_gotiengviet_engine_focus_in;
}
static void ibus_gotiengviet_engine_init(IBusGoTiengVietEngine *e){
    e->preedit=g_string_new("");
    e->modern=TRUE;
    load_config(&e->mode_telex, &e->modern, &e->spellcheck);
}
static IBusBus *bus=NULL;
static IBusFactory *factory=NULL;
static gint engine_id = 0;
static IBusEngine* create_engine_cb(IBusFactory *f, const gchar *engine_name, gpointer user_data){
    gchar *path = g_strdup_printf("/org/freedesktop/IBus/Engine/%d", ++engine_id);
    GDBusConnection *conn = ibus_bus_get_connection(bus);
    IBusEngine *engine = ibus_engine_new_with_type(ibus_gotiengviet_engine_get_type(), engine_name, path, conn);
    g_free(path);
    if(!engine) return NULL;
    g_object_ref_sink(engine);
    IBusGoTiengVietEngine *ue = (IBusGoTiengVietEngine*)engine;
    if(g_strcmp0(engine_name, "gotiengviet-vni")==0) ue->mode_telex=FALSE;
    else ue->mode_telex=TRUE;
    gboolean mod=TRUE, spell=TRUE;
    load_config(NULL, &mod, &spell);
    ue->modern=mod;
    ue->spellcheck=spell;
    return engine;
}
/* Crash handling - chỉ dùng glib hệ thống */
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
static void crash_handler(int sig){
    void *bt[32]; int n = backtrace(bt, 32);
    gchar *dir = g_build_filename(g_get_user_cache_dir(), "gotiengviet", NULL);
    g_mkdir_with_parents(dir, 0755);
    gchar *log = g_build_filename(dir, "crash.log", NULL);
    FILE *f = fopen(log, "a");
    if(f){
        time_t now=time(NULL);
        fprintf(f, "\n=== CRASH %s signal %d (%s) ===\n", ctime(&now), sig, strsignal(sig));
        backtrace_symbols_fd(bt, n, fileno(f));
        fclose(f);
    }
    g_free(dir); g_free(log);
    // Ghi ra stderr để apport bắt
    fprintf(stderr, "GoTiengViet crash signal %d, log saved\n", sig);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);
    // Thử reset preedit thay vì abort cứng
    _exit(1); // để ibus-daemon tự restart engine (watchdog)
}
static void install_crash_handlers(void){
    struct sigaction sa; memset(&sa,0,sizeof(sa));
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    // Bỏ qua SIGPIPE
    signal(SIGPIPE, SIG_IGN);
}

static void bus_connected_cb(IBusBus *b, gpointer user_data){
    IBusComponent *c=ibus_component_new("org.freedesktop.IBus.GoTiengViet","GoTiengViet Engine (thuần hệ thống)","0.1.0","GPL","GoTiengViet Project","https://github.com/isthaison/gotiengviet","/usr/libexec/ibus-engine-gotiengviet --ibus","gotiengviet");
    IBusEngineDesc *d1=ibus_engine_desc_new("gotiengviet-telex","Telex","Telex: s f r x j, aa aw dd - github.com/isthaison/gotiengviet","vi","GPL","GoTiengViet","/usr/share/gotiengviet/icons/gotiengviet.svg","us");
    IBusEngineDesc *d2=ibus_engine_desc_new("gotiengviet-vni","VNI","VNI: 1-5, 6-9, 0 - github.com/isthaison/gotiengviet","vi","GPL","GoTiengViet","/usr/share/gotiengviet/icons/gotiengviet.svg","us");
    ibus_component_add_engine(c,d1);
    ibus_component_add_engine(c,d2);
    ibus_bus_register_component(bus,c);
    if(!factory){
        factory=ibus_factory_new(ibus_bus_get_connection(bus));
        g_signal_connect(factory, "create-engine", G_CALLBACK(create_engine_cb), NULL);
        ibus_factory_add_engine(factory, "gotiengviet-telex", ibus_gotiengviet_engine_get_type());
        ibus_factory_add_engine(factory, "gotiengviet-vni", ibus_gotiengviet_engine_get_type());
    }
    ibus_bus_request_name(bus,"org.freedesktop.IBus.GoTiengViet",0);
}
int main(int argc, char **argv){
    install_crash_handlers();
    init_charset();
    if(argc>1 && strcmp(argv[1],"--ibus")==0){
        ibus_init();
        bus=ibus_bus_new();
        g_signal_connect(bus,"connected",G_CALLBACK(bus_connected_cb),NULL);
        // Nếu bus đã connected sẵn (trường hợp restart nhanh), tạo factory ngay
        if(ibus_bus_is_connected(bus)){
            bus_connected_cb(bus, NULL);
        }
        g_main_loop_run(g_main_loop_new(NULL,FALSE));
    } else {
        printf("GoTiengViet IBus engine - Telex/VNI thuần hệ thống\n");
        printf("Test: as->á, dd->đ, hoaf->hoà\n");
        // quick test using telex_transform
        GString *s=g_string_new("chao");
        GArray *buf=gstring_to_ucs4(s);
        telex_transform(buf,'f',TRUE);
        GString *out=g_string_new("");
        ucs4_to_gstring(buf,out);
        printf("chao+f => %s (expect chào)\n",out->str);
        g_string_free(s,TRUE); g_string_free(out,TRUE); g_array_free(buf,TRUE);
    }
    return 0;
}
