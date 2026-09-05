/* IBus engine - Telex/VNI thuần hệ thống, chỉ dùng libibus-1.0, glib-2.0
 * Build: gcc -O2 -o /usr/libexec/ibus-engine-gotiengviet ibus/engine.c $(pkg-config --cflags --libs ibus-1.0)
 * Không dùng lib ngoài, logic port từ Go engine (charset, phonology, telex)
 */
#include <ibus.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

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

/* ---- Spellcheck & gợi ý: từ điển chung + levenshtein (port từ engine/spell.go) ---- */
static const char *viet_dict_true[] = {
 "chào","xin","cảm","ơn","tiếng","việt","được","hoặc","không","có","của","và","là",
 "trong","người","một","những","các","đã","sẽ","đang","này","kia","đó","gì","nào",
 "sao","thế","rất","quá","lắm","cũng","vẫn","còn","hết","nữa","đi","đến","về",
 "lại","lên","xuống","ra","vào","làm","việc","học","tập","yêu","thương","nhà",
 "trường","lớp","bạn","thầy","cô","gia","đình","bố","mẹ","anh","chị","em","con",
 "ăn","uống","ngủ","chơi","đọc","viết","nghe","nói","biết","hiểu","muốn","thích",
 "cần","phải","nên","đừng","giúp","cho","tặng","mua","bán","tiền","cơm","nước",
 "xe","đường","phố","chợ","sông","núi","biển","cây","hoa","trời","đất","nắng",
 "mưa","gió","ngày","đêm","sáng","tối","giờ","hôm","nay","mai","tuần","tháng",
 "năm","xuân","tết","đẹp","xấu","tốt","mới","cũ","lớn","nhỏ","cao","thấp","dài",
 "ngắn","nhanh","chậm","mạnh","vui","buồn","khỏe","mệt","đói","nóng","lạnh",
 "sạch","ngon","đông","yên","tĩnh","hòa","hoà","hóa","hoá","thưa","thừa",
 "dạ","vâng","ơi","nhé","nha","chứ","mà","thì","rằng","nếu","vì","nhưng",
 "hay","với","giữa","trên","dưới","trước","sau","bên","ngoài","khắp",
 "mọi","mỗi","mấy","bao","nhiêu","đâu","ai","khi","ở","từ","do","để","như",
 "bằng","hơn","nhất","cả","dù","ngay","vừa","đều","bác","bạc","bàn","bàng","bà",
 "hóc","hòn","hòng","hò","công","ca","ke","kê","ki","nga","ngo","ghe","ghê","ga","go",
 "tha","thơm","thơ","test","thử","nghiệm","máy","tính","phần","mềm","hệ","thống",
 "cài","đặt","chính","tả","gợi","ý","dấu","thanh","kiểm","tra","văn","bản",
 "tin","nhắn","điện","thoại","chú","chủ","chúng","tôi","ta","họ","nó","mình",
 NULL
};
/* Từ sai đã biết -> luôn gợi ý sửa */
static const char *viet_dict_false[] = { "hoăc", "bàc", "vièt", "hòc", "kông", "ngha", "chàol", "tếst", "đượck", "thuaw", NULL };

static int dict_lookup(const char *lower_utf8){
    for(int i=0; viet_dict_false[i]; i++) if(strcmp(lower_utf8, viet_dict_false[i])==0) return 0;
    for(int i=0; viet_dict_true[i]; i++) if(strcmp(lower_utf8, viet_dict_true[i])==0) return 1;
    return -1;
}

static gboolean is_valid_coda(const char *coda, int coda_len){
    if(coda_len == 0) return TRUE;
    if(coda_len == 1){
        char c = coda[0];
        return (c=='c' || c=='m' || c=='n' || c=='p' || c=='t');
    }
    if(coda_len == 2){
        return ((coda[0]=='c' && coda[1]=='h') ||
                (coda[0]=='n' && coda[1]=='g') ||
                (coda[0]=='n' && coda[1]=='h'));
    }
    return FALSE;
}

static gboolean is_valid_onset(const gunichar *ucs, glong first_v){
    if(first_v <= 0) return TRUE;
    if(first_v == 1){
        gunichar c = bare_lower(ucs[0]);
        if(c=='b'||c=='c'||c=='d'||c=='g'||c=='h'||c=='k'||c=='l'||c=='m'||
           c=='n'||c=='p'||c=='r'||c=='s'||c=='t'||c=='v'||c=='x'||ucs[0]==0x0111||ucs[0]==0x0110)
            return TRUE;
        return FALSE;
    }
    if(first_v == 2){
        gunichar c0 = bare_lower(ucs[0]), c1 = bare_lower(ucs[1]);
        if(c0=='c'&&c1=='h') return TRUE;
        if(c0=='g'&&c1=='h') return TRUE;
        if(c0=='g'&&c1=='i') return TRUE;
        if(c0=='k'&&c1=='h') return TRUE;
        if(c0=='n'&&c1=='g') return TRUE;
        if(c0=='n'&&c1=='h') return TRUE;
        if(c0=='p'&&c1=='h') return TRUE;
        if(c0=='q'&&c1=='u') return TRUE;
        if(c0=='t'&&c1=='h') return TRUE;
        if(c0=='t'&&c1=='r') return TRUE;
        return FALSE;
    }
    if(first_v == 3){
        gunichar c0 = bare_lower(ucs[0]), c1 = bare_lower(ucs[1]), c2 = bare_lower(ucs[2]);
        if(c0=='n'&&c1=='g'&&c2=='h') return TRUE;
        return FALSE;
    }
    return FALSE;
}

static gboolean spell_word_valid(const char *utf8){
    if(!utf8 || !*utf8) return TRUE;
    glong len = 0;
    gunichar *ucs = g_utf8_to_ucs4(utf8, -1, NULL, &len, NULL);
    if(!ucs) return TRUE;
    if(len < 2){ g_free(ucs); return TRUE; }

    for(glong i=0; i<len; i++){
        if(g_ascii_isdigit((gchar)ucs[i]) || !g_unichar_isalpha(ucs[i])){
            g_free(ucs); return TRUE;
        }
    }

    gchar *lower = g_utf8_strdown(utf8, -1);
    int d = dict_lookup(lower);
    if(d == 0){ g_free(lower); g_free(ucs); return FALSE; }
    if(d == 1){ g_free(lower); g_free(ucs); return TRUE; }
    g_free(lower);

    gboolean has_mark = FALSE;
    for(glong i=0; i<len; i++){
        CharInfo *info = get_info(ucs[i]);
        if(info && (info->diacritic != DIAC_NONE || info->tone != TONE_NONE)){
            has_mark = TRUE; break;
        }
        if(ucs[i] == 0x0111 || ucs[i] == 0x0110){ has_mark = TRUE; break; }
    }
    if(!has_mark){
        gunichar last_c = bare_lower(ucs[len-1]);
        if(last_c == 'w' || last_c == 'f' || last_c == 'j' || last_c == 'z'){
            g_free(ucs); return FALSE;
        }
        g_free(ucs); return TRUE;
    }

    glong first_v = -1, last_v = -1;
    int v_count = 0;
    int tone = TONE_NONE;

    for(glong i=0; i<len; i++){
        if(is_vowel(ucs[i])){
            if(first_v == -1) first_v = i;
            last_v = i;
            v_count++;
        }
        int t = get_tone(ucs[i]);
        if(t != TONE_NONE){
            if(tone != TONE_NONE && tone != t){ g_free(ucs); return FALSE; }
            tone = t;
        }
    }

    if(v_count == 0){ g_free(ucs); return FALSE; }

    glong eff_first_v = first_v;
    if(first_v == 1 && bare_lower(ucs[0]) == 'g' && bare_lower(ucs[1]) == 'i' && len > 2 && is_vowel(ucs[2])){
        eff_first_v = 2;
    } else if(first_v == 1 && bare_lower(ucs[0]) == 'q' && bare_lower(ucs[1]) == 'u' && len > 2 && is_vowel(ucs[2])){
        eff_first_v = 2;
    }
    for(glong i=eff_first_v; i<=last_v; i++){
        if(!is_vowel(ucs[i])){ g_free(ucs); return FALSE; }
    }

    if(!is_valid_onset(ucs, eff_first_v)){ g_free(ucs); return FALSE; }

    gunichar first_v_bare = bare_lower(ucs[eff_first_v]);
    gunichar first_c = bare_lower(ucs[0]);
    if(eff_first_v == 1){
        if(first_c == 'k'){
            if(first_v_bare != 'i' && first_v_bare != 'e' && first_v_bare != 'y'){
                g_free(ucs); return FALSE;
            }
        } else if(first_c == 'c'){
            if(first_v_bare == 'i' || first_v_bare == 'e' || first_v_bare == 'y'){
                g_free(ucs); return FALSE;
            }
        }
    } else if(eff_first_v == 2){
        gunichar second_c = bare_lower(ucs[1]);
        if(first_c == 'g' && second_c == 'h'){
            if(first_v_bare != 'i' && first_v_bare != 'e'){ g_free(ucs); return FALSE; }
        } else if(first_c == 'g' && second_c != 'i'){
            if(first_v_bare == 'e'){ g_free(ucs); return FALSE; }
        } else if(first_c == 'n' && second_c == 'g'){
            if(first_v_bare == 'i' || first_v_bare == 'e' || first_v_bare == 'y'){ g_free(ucs); return FALSE; }
        }
    } else if(eff_first_v == 3){
        if(first_v_bare != 'i' && first_v_bare != 'e' && first_v_bare != 'y'){ g_free(ucs); return FALSE; }
    }

    glong coda_len = len - 1 - last_v;
    char coda_buf[16] = {0};
    if(coda_len > 0){
        if(coda_len > 2){ g_free(ucs); return FALSE; }
        for(glong i=0; i<coda_len; i++){
            coda_buf[i] = (char)bare_lower(ucs[last_v + 1 + i]);
        }
        if(!is_valid_coda(coda_buf, (int)coda_len)){ g_free(ucs); return FALSE; }
    }

    if(coda_len > 0){
        gboolean is_voiceless_stop = FALSE;
        if(coda_len == 1 && (coda_buf[0]=='c' || coda_buf[0]=='p' || coda_buf[0]=='t')) is_voiceless_stop = TRUE;
        if(coda_len == 2 && coda_buf[0]=='c' && coda_buf[1]=='h') is_voiceless_stop = TRUE;

        if(is_voiceless_stop){
            if(tone == TONE_HUYEN || tone == TONE_HOI || tone == TONE_NGA){
                g_free(ucs); return FALSE;
            }
        }
    }

    if(coda_len == 0){
        gunichar last_v_bare = bare_lower(ucs[last_v]);
        int last_v_diac = get_diac(ucs[last_v]);
        if(last_v_bare == 'a' && (last_v_diac == DIAC_BREVE || last_v_diac == DIAC_CIRCUMFLEX)){
            g_free(ucs); return FALSE;
        }
    }

    g_free(ucs);
    return TRUE;
}

static int lev_ucs4(const gunichar *a, glong la, const gunichar *b, glong lb){
    if(la==0) return (int)lb;
    if(lb==0) return (int)la;
    int *prev=g_new(int, lb+1), *cur=g_new(int, lb+1);
    for(glong j=0;j<=lb;j++) prev[j]=(int)j;
    for(glong i=1;i<=la;i++){
        cur[0]=(int)i;
        for(glong j=1;j<=lb;j++){
            int c=(a[i-1]==b[j-1])?0:1;
            int m=prev[j]+1;
            if(cur[j-1]+1<m) m=cur[j-1]+1;
            if(prev[j-1]+c<m) m=prev[j-1]+c;
            cur[j]=m;
        }
        int *t=prev; prev=cur; cur=t;
    }
    int r=prev[lb];
    g_free(prev); g_free(cur);
    return r;
}

static void add_candidate_unique(GPtrArray *out, const char *cand){
    if(!cand || !*cand) return;
    for(guint k=0; k<out->len; k++){
        if(strcmp((char*)out->pdata[k], cand) == 0) return;
    }
    g_ptr_array_add(out, g_strdup(cand));
}

static GPtrArray* get_suggestions(const char *utf8){
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if(!utf8 || !*utf8) return out;
    gchar *lower = g_utf8_strdown(utf8, -1);
    glong llen = 0;
    gunichar *lu = g_utf8_to_ucs4(lower, -1, NULL, &llen, NULL);
    if(!lu){ g_free(lower); return out; }

    // 1. Đặc trị các lỗi gõ Telex kẹt phím ở đuôi:
    if(strcmp(lower, "hoăc") == 0 || strcmp(lower, "hoacw") == 0){
        add_candidate_unique(out, "hoặc");
    }
    if(strcmp(lower, "thuaw") == 0){
        add_candidate_unique(out, "thưa");
        add_candidate_unique(out, "thừa");
    }

    // 2. Thử cắt bỏ phụ âm thừa ở đuôi:
    if(llen >= 3){
        gunichar last_c = bare_lower(lu[llen-1]);
        if(last_c=='s' || last_c=='f' || last_c=='r' || last_c=='x' || last_c=='j' ||
           last_c=='k' || last_c=='l' || last_c=='w' || last_c=='z'){
            gchar *truncated = g_ucs4_to_utf8(lu, llen-1, NULL, NULL, NULL);
            if(truncated){
                if(spell_word_valid(truncated)){
                    add_candidate_unique(out, truncated);
                }
                g_free(truncated);
            }
        }
    }

    // 3. Đổi dấu thanh cho âm tắc c, ch, p, t:
    glong first_v = -1, last_v = -1;
    int tone_pos = -1;
    for(glong i=0; i<llen; i++){
        if(is_vowel(lu[i])){
            if(first_v == -1) first_v = i;
            last_v = i;
            if(get_tone(lu[i]) != TONE_NONE) tone_pos = (int)i;
        }
    }
    if(last_v != -1 && last_v < llen - 1 && tone_pos != -1){
        CharInfo *v_info = get_info(lu[tone_pos]);
        if(v_info && (v_info->tone == TONE_HUYEN || v_info->tone == TONE_HOI || v_info->tone == TONE_NGA)){
            int try_tones[] = {TONE_SAC, TONE_NANG, 0};
            for(int ti=0; try_tones[ti]; ti++){
                gunichar new_v;
                if(lookup_char(v_info->bare, v_info->diacritic, try_tones[ti], v_info->is_upper, &new_v)){
                    gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
                    alt_ucs[tone_pos] = new_v;
                    gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
                    if(alt_str){
                        if(spell_word_valid(alt_str)){
                            add_candidate_unique(out, alt_str);
                        }
                        g_free(alt_str);
                    }
                    g_free(alt_ucs);
                }
            }
        }
    }

    // 4. Sửa phụ âm đầu k/c, ngh/ng, gh/g:
    if(llen >= 2 && bare_lower(lu[0]) == 'k'){
        gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
        alt_ucs[0] = 'c';
        gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
        if(alt_str){
            if(spell_word_valid(alt_str)) add_candidate_unique(out, alt_str);
            g_free(alt_str);
        }
        g_free(alt_ucs);
    } else if(llen >= 3 && bare_lower(lu[0]) == 'n' && bare_lower(lu[1]) == 'g' && bare_lower(lu[2]) == 'h'){
        gunichar *alt_ucs = g_new(gunichar, llen - 1);
        alt_ucs[0] = 'n'; alt_ucs[1] = 'g';
        for(glong i=3; i<llen; i++) alt_ucs[i-1] = lu[i];
        gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen - 1, NULL, NULL, NULL);
        if(alt_str){
            if(spell_word_valid(alt_str)) add_candidate_unique(out, alt_str);
            g_free(alt_str);
        }
        g_free(alt_ucs);
    }

    // 5. Tìm trong từ điển bằng khoảng cách Levenshtein:
    for(int i=0; viet_dict_true[i] && out->len < 5; i++){
        glong dlen = 0;
        gunichar *du = g_utf8_to_ucs4(viet_dict_true[i], -1, NULL, &dlen, NULL);
        if(!du) continue;
        glong diff = llen > dlen ? llen - dlen : dlen - llen;
        if(diff <= 2 && lev_ucs4(lu, llen, du, dlen) <= 2){
            add_candidate_unique(out, viet_dict_true[i]);
        }
        g_free(du);
    }

    g_free(lu); g_free(lower);
    return out;
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
struct _GoTiengVietEngine { IBusEngine parent; GString *preedit; gboolean mode_telex; gboolean modern; gboolean spellcheck; gchar **candidates; int n_candidates; int cand_cursor; };
struct _GoTiengVietEngineClass { IBusEngineClass parent; };
G_DEFINE_TYPE(IBusGoTiengVietEngine, ibus_gotiengviet_engine, IBUS_TYPE_ENGINE)

/* ---- Macro & emoji: gõ tắt + space -> mở rộng (port từ engine/macro.go) ---- */
static const char *macro_pairs[][2] = {
    {"vn","Việt Nam"}, {"hn","Hà Nội"}, {"hcm","Hồ Chí Minh"},
    {"dc","được"}, {"ko","không"}, {"ntn","như thế nào"},
    {"cx","cũng"}, {"cb","chuẩn bị"},
    {NULL, NULL}
};
static const char *emoji_pairs[][2] = {
    {":smile:","😊"}, {":heart:","❤️"}, {":laugh:","😂"},
    {":sad:","😢"}, {":angry:","😠"}, {":thumbsup:","👍"},
    {":fire:","🔥"}, {":star:","⭐"}, {":check:","✅"}, {":vim:","💚"},
    {NULL, NULL}
};
/* Trả về chuỗi mới (caller g_free) — đã expand macro/emoji, hoặc bản sao nguyên văn */
static gchar* expand_word(const char *word){
    if(!word) return g_strdup("");
    gchar *lower=g_utf8_strdown(word,-1);
    for(int i=0; macro_pairs[i][0]; i++){
        if(strcmp(lower, macro_pairs[i][0])==0){ gchar *r=g_strdup(macro_pairs[i][1]); g_free(lower); return r; }
    }
    for(int i=0; emoji_pairs[i][0]; i++){
        if(strcmp(word, emoji_pairs[i][0])==0 || strcmp(lower, emoji_pairs[i][0])==0){
            gchar *r=g_strdup(emoji_pairs[i][1]); g_free(lower); return r;
        }
    }
    g_free(lower);
    return g_strdup(word);
}
static void clear_candidates(IBusGoTiengVietEngine *e){
    if(e->candidates){
        for(int i=0;i<e->n_candidates;i++) g_free(e->candidates[i]);
        g_free(e->candidates);
        e->candidates=NULL;
    }
    e->n_candidates=0;
    e->cand_cursor=0;
}
static void hide_suggest(IBusGoTiengVietEngine *e, IBusEngine *engine){
    clear_candidates(e);
    ibus_engine_hide_lookup_table(engine);
}
/* Đẩy preedit + gạch đỏ từ sai + bảng gợi ý (Tab chọn, Up/Down di chuyển, Esc bỏ) */
static void push_preedit(IBusGoTiengVietEngine *e, IBusEngine *engine, guint cursor, gboolean visible){
    glong plen=g_utf8_strlen(e->preedit->str, -1);
    gboolean bad=(e->spellcheck && plen>=2 && !spell_word_valid(e->preedit->str));
    IBusText *t=ibus_text_new_from_string(e->preedit->str);
    if(bad) ibus_text_append_attribute(t, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_ERROR, 0, (gint)plen);
    ibus_engine_update_preedit_text(engine,t,cursor,visible);
    clear_candidates(e);
    if(!bad){ ibus_engine_hide_lookup_table(engine); return; }
    GPtrArray *sugs=get_suggestions(e->preedit->str);
    if(sugs->len==0){ ibus_engine_hide_lookup_table(engine); g_ptr_array_free(sugs,TRUE); return; }
    e->n_candidates=(int)sugs->len;
    e->cand_cursor=0;
    e->candidates=g_new(gchar*, sugs->len);
    IBusLookupTable *table=ibus_lookup_table_new(5, 0, TRUE, FALSE);
    for(guint i=0;i<sugs->len;i++){
        e->candidates[i]=g_strdup((char*)sugs->pdata[i]);
        ibus_lookup_table_append_candidate(table, ibus_text_new_from_string(e->candidates[i]));
    }
    g_ptr_array_free(sugs,TRUE);
    ibus_engine_update_lookup_table(engine, table, TRUE);
    g_object_unref(table);
}
static void ibus_gotiengviet_engine_reset(IBusGoTiengVietEngine *e){
    if(e->preedit) g_string_assign(e->preedit,"");
}
// Tray đổi method khi đang gõ không gây focus_in, nên reload config theo mtime mỗi phím
static time_t cfg_mtime_cache = 0;
static void reload_config_if_changed(IBusGoTiengVietEngine *e){
    gchar *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    struct stat st;
    if(stat(path, &st) == 0 && st.st_mtime != cfg_mtime_cache){
        cfg_mtime_cache = st.st_mtime;
        // load_config đã có ở dưới (forward); gọi trực tiếp qua GKeyFile để tránh phụ thuộc thứ tự
        GKeyFile *kf = g_key_file_new();
        if(g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)){
            gchar *m = g_key_file_get_string(kf, "input", "method", NULL);
            if(m){ e->mode_telex = (g_strcmp0(m, "vni") != 0 && g_strcmp0(m, "VNI") != 0); g_free(m); }
            gchar *mo = g_key_file_get_string(kf, "input", "modern", NULL);
            if(mo){ e->modern = !(g_strcmp0(mo, "false") == 0 || g_strcmp0(mo, "0") == 0); g_free(mo); }
            gchar *sp = g_key_file_get_string(kf, "input", "spellcheck", NULL);
            if(sp){ e->spellcheck = !(g_strcmp0(sp, "false") == 0 || g_strcmp0(sp, "0") == 0); g_free(sp); }
        }
        g_key_file_free(kf);
    }
    g_free(path);
}
static gboolean ibus_gotiengviet_engine_process_key_event(IBusEngine *engine, guint keyval, guint keycode, guint modifiers){
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    reload_config_if_changed(e);
    if(modifiers & IBUS_RELEASE_MASK) return FALSE;
    // Phím tắt Ctrl/Alt/Super (Ctrl+C/V/X/Z, Ctrl+S...) — commit chữ đang dở rồi nhường cho app
    if(modifiers & (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK | IBUS_HYPER_MASK | IBUS_META_MASK)){
        if(e->preedit->len>0){
            IBusText *t=ibus_text_new_from_string(e->preedit->str);
            ibus_engine_commit_text(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
        }
        return FALSE;
    }
    // Tab chọn gợi ý, 1..5 chọn nhanh, Up/Down di chuyển, Enter commit gợi ý, Esc bỏ bảng gợi ý
    if(e->n_candidates>0 && e->candidates){
        if(keyval==IBUS_Tab){
            g_string_assign(e->preedit, e->candidates[e->cand_cursor]);
            push_preedit(e, engine, e->preedit->len, TRUE);
            return TRUE;
        }
        if((keyval>=IBUS_1 && keyval<=IBUS_5) || (keyval>=IBUS_KP_1 && keyval<=IBUS_KP_5)){
            int idx = (keyval>=IBUS_1 && keyval<=IBUS_5) ? (keyval - IBUS_1) : (keyval - IBUS_KP_1);
            if(idx < e->n_candidates){
                g_string_assign(e->preedit, e->candidates[idx]);
                gchar *word=expand_word(e->preedit->str);
                IBusText *t=ibus_text_new_from_string(word);
                g_free(word);
                ibus_engine_commit_text(engine,t);
                ibus_gotiengviet_engine_reset(e);
                IBusText *empty=ibus_text_new_from_string("");
                ibus_engine_update_preedit_text(engine,empty,0,FALSE);
                hide_suggest(e, engine);
                return TRUE;
            }
        }
        if(keyval==IBUS_Up || keyval==IBUS_Down){
            if(keyval==IBUS_Down) e->cand_cursor=(e->cand_cursor+1)%e->n_candidates;
            else e->cand_cursor=(e->cand_cursor+e->n_candidates-1)%e->n_candidates;
            IBusLookupTable *table=ibus_lookup_table_new(5, (guint)e->cand_cursor, TRUE, FALSE);
            for(int i=0;i<e->n_candidates;i++)
                ibus_lookup_table_append_candidate(table, ibus_text_new_from_string(e->candidates[i]));
            ibus_engine_update_lookup_table(engine, table, TRUE);
            g_object_unref(table);
            return TRUE;
        }
        if(keyval==IBUS_Return || keyval==IBUS_KP_Enter){
            g_string_assign(e->preedit, e->candidates[e->cand_cursor]);
            gchar *word=expand_word(e->preedit->str);
            IBusText *t=ibus_text_new_from_string(word);
            g_free(word);
            ibus_engine_commit_text(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        if(keyval==IBUS_Escape){
            hide_suggest(e, engine);
            return TRUE;
        }
    }
    // Backspace
    if(keyval==IBUS_BackSpace){
        if(e->preedit->len>0){
            // remove last utf8 char
            gchar *prev = g_utf8_prev_char(e->preedit->str + e->preedit->len);
            g_string_truncate(e->preedit, prev - e->preedit->str);
            push_preedit(e, engine, e->preedit->len, TRUE);
            return TRUE;
        }
        return FALSE;
    }
    if(keyval==IBUS_space){
        if(e->preedit->len>0){
            gchar *commit = expand_word(e->preedit->str);
            gchar *with_space = g_strdup_printf("%s ",commit);
            IBusText *t=ibus_text_new_from_string(with_space);
            ibus_engine_commit_text(engine,t);
            g_free(commit); g_free(with_space);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        return FALSE;
    }
    // punctuation commit
    if((keyval>=IBUS_exclam && keyval<=IBUS_slash) || (keyval>=IBUS_colon && keyval<=IBUS_at) || (keyval>=IBUS_bracketleft && keyval<=IBUS_grave) || (keyval>=IBUS_braceleft && keyval<=IBUS_asciitilde)){
        // if preedit has content, commit it + punctuation
        if(e->preedit->len>0 && (keyval==IBUS_comma || keyval==IBUS_period || keyval==IBUS_question || keyval==IBUS_exclam || keyval==IBUS_colon || keyval==IBUS_semicolon)){
            gchar c=(gchar)keyval;
            gchar *word=expand_word(e->preedit->str);
            gchar *commit=g_strdup_printf("%s%c",word,c);
            g_free(word);
            IBusText *t=ibus_text_new_from_string(commit);
            ibus_engine_commit_text(engine,t);
            g_free(commit);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
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
            push_preedit(e, engine, e->preedit->len, TRUE);
            g_array_free(buf,TRUE);
            return TRUE;
        }
        g_array_free(buf,TRUE);
        // not consumed -> append
        g_string_append_c(e->preedit,c);
        push_preedit(e, engine, e->preedit->len, TRUE);
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
            push_preedit(e, engine, e->preedit->len, TRUE);
            g_array_free(buf,TRUE);
            return TRUE;
        }
        g_array_free(buf,TRUE);
        g_string_append_c(e->preedit,c);
        push_preedit(e, engine, e->preedit->len, TRUE);
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
                push_preedit(e, engine, e->preedit->len, TRUE);
                g_array_free(buf,TRUE);
                return TRUE;
            }
            g_array_free(buf,TRUE);
        }
        // if not consumed, append digit (or commit?)
        g_string_append_c(e->preedit,c);
        push_preedit(e, engine, e->preedit->len, TRUE);
        return TRUE;
    }
    // Enter -> commit
    if(keyval==IBUS_Return || keyval==IBUS_KP_Enter){
        if(e->preedit->len>0){
            gchar *word=expand_word(e->preedit->str);
            IBusText *t=ibus_text_new_from_string(word);
            g_free(word);
            ibus_engine_commit_text(engine,t);
            ibus_gotiengviet_engine_reset(e);
            IBusText *empty=ibus_text_new_from_string("");
            ibus_engine_update_preedit_text(engine,empty,0,FALSE);
            hide_suggest(e, engine);
            return TRUE;
        }
        return FALSE;
    }
    // Other keys: commit preedit and forward
    if(e->preedit->len>0){
        gchar *word=expand_word(e->preedit->str);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        ibus_engine_commit_text(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
        hide_suggest(e, engine);
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
    gboolean telex, modern, spell;
    load_config(&telex, &modern, &spell);
    e->mode_telex=telex;
    e->modern=modern;
    e->spellcheck=spell;
    // Một engine duy nhất "gotiengviet": chuyển Telex/VNI trên indicator của app GoTiengViet
    // Đồng bộ cache mtime để reload_config_if_changed không load lại ngay
    gchar *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    struct stat st;
    if(stat(path, &st) == 0) cfg_mtime_cache = st.st_mtime;
    g_free(path);
    hide_suggest(e, engine);
}
static void ibus_gotiengviet_engine_candidate_clicked(IBusEngine *engine, guint index, guint button, guint state){
    (void)button; (void)state;
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    if(index < (guint)e->n_candidates && e->candidates){
        g_string_assign(e->preedit, e->candidates[index]);
        gchar *word=expand_word(e->preedit->str);
        IBusText *t=ibus_text_new_from_string(word);
        g_free(word);
        ibus_engine_commit_text(engine,t);
        ibus_gotiengviet_engine_reset(e);
        IBusText *empty=ibus_text_new_from_string("");
        ibus_engine_update_preedit_text(engine,empty,0,FALSE);
        hide_suggest(e, engine);
    }
}
static void ibus_gotiengviet_engine_class_init(IBusGoTiengVietEngineClass *klass){
    IBusEngineClass *ec=IBUS_ENGINE_CLASS(klass);
    ec->process_key_event=ibus_gotiengviet_engine_process_key_event;
    ec->focus_in=ibus_gotiengviet_engine_focus_in;
    ec->candidate_clicked=ibus_gotiengviet_engine_candidate_clicked;
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
    // Một engine duy nhất "gotiengviet"; giữ tương thích tên cũ khi user còn sót config
    gboolean telex = TRUE, mod = TRUE, spell = TRUE;
    load_config(&telex, &mod, &spell);
    if(g_strcmp0(engine_name, "gotiengviet-vni") == 0) telex = FALSE;
    else if(g_strcmp0(engine_name, "gotiengviet-telex") == 0) telex = TRUE;
    ue->mode_telex = telex;
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
    IBusEngineDesc *d=ibus_engine_desc_new("gotiengviet","GoTiengViet","GoTiengViet: Telex/VNI (đổi Telex/VNI trên indicator của app) - github.com/isthaison/gotiengviet","vi","GPL","GoTiengViet","gotiengviet","us");
    ibus_component_add_engine(c,d);
    ibus_bus_register_component(bus,c);
    if(!factory){
        factory=ibus_factory_new(ibus_bus_get_connection(bus));
        g_signal_connect(factory, "create-engine", G_CALLBACK(create_engine_cb), NULL);
        ibus_factory_add_engine(factory, "gotiengviet", ibus_gotiengviet_engine_get_type());
        // Giữ tên cũ để máy đã cài không mất engine khi chưa re-login
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
