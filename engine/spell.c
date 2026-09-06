#include "internal.h"

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

int dict_lookup(const char *lower_utf8){
    for(int i=0; viet_dict_false[i]; i++) if(strcmp(lower_utf8, viet_dict_false[i])==0) return 0;
    for(int i=0; viet_dict_true[i]; i++) if(strcmp(lower_utf8, viet_dict_true[i])==0) return 1;
    return -1;
}

gboolean is_valid_coda(const char *coda, int coda_len){
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

gboolean is_valid_onset(const gunichar *ucs, glong first_v){
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

gboolean spell_word_valid(const char *utf8){
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

int lev_ucs4(const gunichar *a, glong la, const gunichar *b, glong lb){
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

void add_candidate_unique(GPtrArray *out, const char *cand){
    if(!cand || !*cand) return;
    for(guint k=0; k<out->len; k++){
        if(strcmp((char*)out->pdata[k], cand) == 0) return;
    }
    g_ptr_array_add(out, g_strdup(cand));
}

GPtrArray* get_suggestions(const char *utf8){
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
