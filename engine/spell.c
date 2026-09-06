#include "internal.h"

/* Từ điển tiếng Việt phổ biến dùng cho tra cứu nhanh và gợi ý chính tả */
static const char *viet_dict_true[] = {
    /* Đại từ, số từ, trợ từ, liên từ */
    "tôi", "ta", "tao", "chúng", "mình", "bạn", "cậu", "họ", "nó", "ai", "gì", "nào", "đâu", "sao", "thế",
    "ông", "bà", "bác", "chú", "cô", "dì", "anh", "chị", "em", "con", "cháu", "mẹ", "bố", "cha", "thầy",
    "một", "hai", "ba", "bốn", "năm", "sáu", "bảy", "tám", "chín", "mười", "trăm", "nghìn", "ngàn", "triệu", "tỷ",
    "và", "hoặc", "hay", "nhưng", "mà", "thì", "là", "rằng", "vì", "do", "nếu", "hễ", "tuy", "dù", "để", "cho",
    "với", "cùng", "của", "trong", "ngoài", "trên", "dưới", "trước", "sau", "giữa", "bên", "cạnh", "khắp", "mọi",
    "mỗi", "từng", "mấy", "bao", "nhiêu", "những", "các", "cả", "đều", "ngay", "vừa", "mới", "đã", "đang", "sẽ",
    "chưa", "chẳng", "không", "chớ", "đừng", "hãy", "quá", "lắm", "rất", "cực", "hơi", "khá", "hơn", "nhất", "cũng",
    "vẫn", "còn", "hết", "nữa", "luôn", "thường", "ít", "nhiều", "ơi", "à", "ạ", "nhé", "nha", "chứ", "nhỉ", "vâng", "dạ",

    /* Động từ và tính từ thông dụng */
    "ăn", "uống", "ngủ", "nghỉ", "nghĩ", "thức", "chơi", "làm", "việc", "học", "hành", "tập", "đọc", "viết",
    "nghe", "nói", "hỏi", "bảo", "kể", "nhìn", "thấy", "xem", "biết", "hiểu", "muốn", "thích", "yêu", "thương",
    "ghét", "nhớ", "quên", "sợ", "lo", "buồn", "vui", "mừng", "cười", "khóc", "đi", "đến", "về", "lại", "qua",
    "lên", "xuống", "ra", "vào", "tới", "lui", "chạy", "nhảy", "bước", "ngồi", "đứng", "nằm", "bay", "bơi",
    "mua", "bán", "tặng", "cho", "nhận", "lấy", "mang", "cầm", "nắm", "giữ", "đặt", "để", "bỏ", "gửi", "tìm",
    "kiếm", "gặp", "thấy", "giúp", "đỡ", "cần", "phải", "nên", "mở", "đóng", "bật", "tắt", "bắt", "đầu", "kết",
    "thúc", "dừng", "tiếp", "tục", "xây", "dựng", "sửa", "chữa", "phát", "triển", "quản", "lý", "bảo", "vệ",
    "tốt", "xấu", "đẹp", "mới", "cũ", "lớn", "nhỏ", "to", "bé", "cao", "thấp", "dài", "ngắn", "rộng", "hẹp",
    "nhanh", "chậm", "mạnh", "yếu", "nặng", "nhẹ", "dễ", "khó", "đúng", "sai", "thật", "giả", "giàu", "nghèo",
    "ấm", "nóng", "lạnh", "mát", "đói", "no", "mệt", "khỏe", "sạch", "bẩn", "ngon", "ngọt", "cay", "đắng", "chua",
    "chát", "mặn", "nhạt", "đậm", "rõ", "mờ", "sáng", "tối", "đông", "vắng", "yên", "tĩnh", "ồn", "đầy", "vơi",
    "tròn", "vuông", "thẳng", "cong", "xa", "gần", "sớm", "muộn", "khác", "giống", "quen", "lạ",

    /* Danh từ tự nhiên, thời gian, xã hội, kỹ thuật */
    "nhà", "cửa", "phòng", "bàn", "ghế", "giường", "bếp", "sân", "vườn", "trường", "lớp", "chợ", "đường", "phố",
    "xe", "cộ", "sông", "núi", "biển", "hồ", "rừng", "cây", "hoa", "lá", "quả", "trái", "đất", "nước", "lửa",
    "trời", "mây", "mưa", "nắng", "gió", "bão", "sương", "tuyết", "ngày", "đêm", "sáng", "trưa", "chiều", "tối",
    "hôm", "nay", "mai", "kia", "qua", "giờ", "phút", "giây", "tuần", "tháng", "năm", "mùa", "xuân", "hạ", "thu",
    "đông", "tết", "tiền", "bạc", "vàng", "cơm", "gạo", "thịt", "cá", "rau", "áo", "quần", "giày", "dép", "nón",
    "sách", "vở", "bút", "mực", "giấy", "báo", "tranh", "ảnh", "máy", "tính", "điện", "thoại", "phần", "mềm",
    "hệ", "thống", "tin", "nhắn", "chính", "tả", "văn", "bản", "gợi", "ý", "dấu", "thanh", "kiểm", "tra", "cài",
    "đặt", "chương", "trình", "mạng", "dữ", "liệu", "thông", "tin", "công", "nghệ", "khoa", "học", "kỹ", "thuật",
    "tiếng", "việt", "quốc", "gia", "nhân", "dân", "xã", "hội", "kinh", "tế", "văn", "hóa", "giáo", "dục",
    "y", "tế", "luật", "pháp", "nhà", "nước", "chính", "phủ", "đoàn", "thể", "cộng", "đồng",

    /* Các cặp từ âm chuẩn dễ nhầm lẫn */
    "sắp", "sẵn", "sống", "sinh", "suốt", "suất", "xuất", "xanh", "xinh", "xong", "xưa", "xuân", "xơ", "xóm",
    "trên", "trong", "trước", "trời", "trẻ", "trăng", "tròn", "chân", "chơi", "cho", "chung", "chút", "chưa", "chờ",
    "dành", "giành", "rành", "dạy", "dễ", "dẫn", "giờ", "giúp", "giữa", "giữ", "giải", "giá", "rồi", "rất", "rõ",
    "nghĩ", "nghỉ", "sữa", "sửa", "bão", "bảo", "vẽ", "vẻ", "cũng", "đỗ", "đổ", "vỡ", "vở", "kỹ", "mã", "mả",
    "công", "cùng", "cuộc", "ke", "kê", "ki", "kẹo", "kinh", "kiến", "kiên", "khi", "khó", "khoa", "khác",
    "ga", "go", "gu", "gà", "gần", "gạo", "gặp", "ghe", "ghế", "ghé", "ghét", "ghi", "ghim",
    "nga", "ngo", "ngu", "ngày", "người", "ngon", "ngủ", "ngắn", "nghe", "nghề", "nghèo", "nghiên",
    "qua", "quan", "quân", "quen", "quét", "quý", "quang", "quanh", "quyết",
    "thưa", "thừa", "được", "bác", "bạc", "học", "hóc", "chào", "cảm", "ơn",
    NULL
};

/* Bản đồ các lỗi gõ sai / kẹt phím Telex/VNI kinh điển -> gợi ý trực tiếp */
static const struct {
    const char *bad;
    const char *good[3];
} common_fixes[] = {
    {"hoăc", {"hoặc", NULL}},
    {"hoacw", {"hoặc", NULL}},
    {"thuaw", {"thưa", "thừa", NULL}},
    {"bàc", {"bác", "bạc", NULL}},
    {"hòc", {"học", "hóc", NULL}},
    {"vièt", {"việt", NULL}},
    {"kông", {"không", "công", NULL}},
    {"ngha", {"nga", NULL}},
    {"ngho", {"ngo", NULL}},
    {"nghu", {"ngu", NULL}},
    {"gha", {"ga", NULL}},
    {"gho", {"go", NULL}},
    {"ghu", {"gu", NULL}},
    {"ge", {"ghe", NULL}},
    {"gê", {"ghê", NULL}},
    {"chàol", {"chào", NULL}},
    {"tếst", {"tét", "test", NULL}},
    {"đượck", {"được", NULL}},
    {"duocw", {"được", "dược", NULL}},
    {"duocwjd", {"được", NULL}},
    {"duocjwd", {"được", NULL}},
    {"bieetw", {"biết", NULL}},
    {"nghiw", {"nghĩ", "nghỉ", NULL}},
    {"dduoc", {"được", NULL}},
    {"dduowjc", {"được", NULL}},
    {"duowjc", {"dược", "được", NULL}},
    {"khong", {"không", NULL}},
    {"duoc", {"được", NULL}},
    {NULL, {NULL}}
};

int dict_lookup(const char *lower_utf8){
    if(!lower_utf8 || !*lower_utf8) return -1;
    for(int i=0; common_fixes[i].bad; i++){
        if(strcmp(lower_utf8, common_fixes[i].bad) == 0) return 0;
    }
    for(int i=0; viet_dict_true[i]; i++){
        if(strcmp(lower_utf8, viet_dict_true[i]) == 0) return 1;
    }
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

/* Áp dụng kiểu chữ hoa/thường của từ gốc lên từ gợi ý */
static gchar *apply_casing(const char *orig, const char *cand){
    if(!orig || !cand) return g_strdup(cand ? cand : "");
    glong olen = 0, clen = 0;
    gunichar *ou = g_utf8_to_ucs4(orig, -1, NULL, &olen, NULL);
    gunichar *cu = g_utf8_to_ucs4(cand, -1, NULL, &clen, NULL);
    if(!ou || !cu){
        g_free(ou); g_free(cu);
        return g_strdup(cand);
    }
    gboolean all_upper = TRUE;
    gboolean first_upper = g_unichar_isupper(ou[0]);
    for(glong i=0; i<olen; i++){
        if(g_unichar_isalpha(ou[i]) && !g_unichar_isupper(ou[i])){
            all_upper = FALSE;
            break;
        }
    }
    if(all_upper && olen > 1){
        for(glong i=0; i<clen; i++) cu[i] = g_unichar_toupper(cu[i]);
    } else if(first_upper && clen > 0){
        cu[0] = g_unichar_toupper(cu[0]);
    }
    gchar *res = g_ucs4_to_utf8(cu, clen, NULL, NULL, NULL);
    g_free(ou); g_free(cu);
    return res ? res : g_strdup(cand);
}

typedef struct {
    char *word;
    int score;
} ScoredCand;

static void add_candidate_scored(GArray *items, const char *cand, int score){
    if(!cand || !*cand) return;
    for(guint i=0; i<items->len; i++){
        ScoredCand *ci = &g_array_index(items, ScoredCand, i);
        if(strcmp(ci->word, cand) == 0){
            if(score > ci->score) ci->score = score;
            return;
        }
    }
    ScoredCand ci;
    ci.word = g_strdup(cand);
    ci.score = score;
    g_array_append_val(items, ci);
}

static gint compare_scored_cand(gconstpointer a, gconstpointer b){
    const ScoredCand *ca = a;
    const ScoredCand *cb = b;
    if(cb->score != ca->score) return cb->score - ca->score;
    return strcmp(ca->word, cb->word);
}

GPtrArray* get_suggestions(const char *utf8){
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if(!utf8 || !*utf8) return out;

    gchar *lower = g_utf8_strdown(utf8, -1);
    glong llen = 0;
    gunichar *lu = g_utf8_to_ucs4(lower, -1, NULL, &llen, NULL);
    if(!lu){ g_free(lower); return out; }

    GArray *items = g_array_new(FALSE, FALSE, sizeof(ScoredCand));

    /* 1. Tra cứu bảng lỗi chính tả và gõ phím đã biết */
    for(int i=0; common_fixes[i].bad; i++){
        if(strcmp(lower, common_fixes[i].bad) == 0){
            for(int j=0; common_fixes[i].good[j]; j++){
                add_candidate_scored(items, common_fixes[i].good[j], 100);
            }
            break;
        }
    }

    /* 2. Quy tắc chính tả phụ âm đầu tiếng Việt (k/c, gh/g, ngh/ng, qu) */
    if(llen >= 2 && bare_lower(lu[0]) == 'k'){
        gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
        alt_ucs[0] = 'c';
        gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
        if(alt_str){
            if(spell_word_valid(alt_str)) add_candidate_scored(items, alt_str, 95);
            g_free(alt_str);
        }
        g_free(alt_ucs);
    } else if(llen >= 2 && bare_lower(lu[0]) == 'c'){
        gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
        alt_ucs[0] = 'k';
        gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
        if(alt_str){
            if(spell_word_valid(alt_str)) add_candidate_scored(items, alt_str, 95);
            g_free(alt_str);
        }
        g_free(alt_ucs);
    } else if(llen >= 2 && bare_lower(lu[0]) == 'g' && bare_lower(lu[1]) != 'h' && bare_lower(lu[1]) != 'i'){
        /* g -> gh trước e, ê */
        gunichar v = bare_lower(lu[1]);
        if(v == 'e' || v == 0x00ea){
            gunichar *alt_ucs = g_new(gunichar, llen + 1);
            alt_ucs[0] = 'g'; alt_ucs[1] = 'h';
            for(glong i=1; i<llen; i++) alt_ucs[i+1] = lu[i];
            gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen + 1, NULL, NULL, NULL);
            if(alt_str){
                if(spell_word_valid(alt_str)) add_candidate_scored(items, alt_str, 95);
                g_free(alt_str);
            }
            g_free(alt_ucs);
        }
    } else if(llen >= 3 && bare_lower(lu[0]) == 'g' && bare_lower(lu[1]) == 'h'){
        /* gh -> g trước a, o, u */
        gunichar *alt_ucs = g_new(gunichar, llen - 1);
        alt_ucs[0] = 'g';
        for(glong i=2; i<llen; i++) alt_ucs[i-1] = lu[i];
        gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen - 1, NULL, NULL, NULL);
        if(alt_str){
            if(spell_word_valid(alt_str)) add_candidate_scored(items, alt_str, 95);
            g_free(alt_str);
        }
        g_free(alt_ucs);
    } else if(llen >= 3 && bare_lower(lu[0]) == 'n' && bare_lower(lu[1]) == 'g' && bare_lower(lu[2]) == 'h'){
        /* ngh -> ng trước a, o, u */
        gunichar *alt_ucs = g_new(gunichar, llen - 1);
        alt_ucs[0] = 'n'; alt_ucs[1] = 'g';
        for(glong i=3; i<llen; i++) alt_ucs[i-1] = lu[i];
        gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen - 1, NULL, NULL, NULL);
        if(alt_str){
            if(spell_word_valid(alt_str)) add_candidate_scored(items, alt_str, 95);
            g_free(alt_str);
        }
        g_free(alt_ucs);
    } else if(llen >= 2 && bare_lower(lu[0]) == 'n' && bare_lower(lu[1]) == 'g'){
        /* ng -> ngh trước e, ê, i */
        gunichar v = bare_lower(lu[2]);
        if(v == 'e' || v == 'i' || v == 'y' || v == 0x00ea){
            gunichar *alt_ucs = g_new(gunichar, llen + 1);
            alt_ucs[0] = 'n'; alt_ucs[1] = 'g'; alt_ucs[2] = 'h';
            for(glong i=2; i<llen; i++) alt_ucs[i+1] = lu[i];
            gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen + 1, NULL, NULL, NULL);
            if(alt_str){
                if(spell_word_valid(alt_str)) add_candidate_scored(items, alt_str, 95);
                g_free(alt_str);
            }
            g_free(alt_ucs);
        }
    }

    /* 3. Xử lý phụ âm dính ở đuôi do gõ nhanh (Telex / typo) */
    if(llen >= 3){
        gunichar last_c = bare_lower(lu[llen-1]);
        if(last_c=='s' || last_c=='f' || last_c=='r' || last_c=='x' || last_c=='j' ||
           last_c=='k' || last_c=='l' || last_c=='w' || last_c=='z'){
            gchar *truncated = g_ucs4_to_utf8(lu, llen-1, NULL, NULL, NULL);
            if(truncated){
                if(spell_word_valid(truncated)){
                    add_candidate_scored(items, truncated, 90);
                }
                /* Thử chuyển đổi dấu đuôi nếu từ trước đó nhận được dấu */
                gchar *recomposed = gtv_transform(lower, GTV_TELEX, TRUE);
                if(recomposed){
                    if(strcmp(recomposed, lower) != 0 && spell_word_valid(recomposed)){
                        add_candidate_scored(items, recomposed, 92);
                    }
                    g_free(recomposed);
                }
                g_free(truncated);
            }
        }
    }

    /* 4. Sửa thanh điệu cho âm tiết kết thúc bằng âm tắc (c, ch, p, t chỉ đi với sắc/nặng) */
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
                            add_candidate_scored(items, alt_str, 85);
                        }
                        g_free(alt_str);
                    }
                    g_free(alt_ucs);
                }
            }
        }
    }

    /* 5. Gợi ý biến thể ngữ âm dễ nhầm lẫn theo phương ngữ (Hỏi <-> Ngã, s <-> x, tr <-> ch, d <-> gi) */
    if(tone_pos != -1){
        CharInfo *v_info = get_info(lu[tone_pos]);
        if(v_info){
            int alt_tone = TONE_NONE;
            if(v_info->tone == TONE_HOI) alt_tone = TONE_NGA;
            else if(v_info->tone == TONE_NGA) alt_tone = TONE_HOI;

            if(alt_tone != TONE_NONE){
                gunichar new_v;
                if(lookup_char(v_info->bare, v_info->diacritic, alt_tone, v_info->is_upper, &new_v)){
                    gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
                    alt_ucs[tone_pos] = new_v;
                    gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
                    if(alt_str){
                        if(spell_word_valid(alt_str)){
                            add_candidate_scored(items, alt_str, 80);
                        }
                        g_free(alt_str);
                    }
                    g_free(alt_ucs);
                }
            }
        }
    }
    if(llen >= 2){
        if(bare_lower(lu[0]) == 's'){
            gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
            alt_ucs[0] = 'x';
            gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
            if(alt_str){
                if(dict_lookup(alt_str) == 1) add_candidate_scored(items, alt_str, 78);
                g_free(alt_str);
            }
            g_free(alt_ucs);
        } else if(bare_lower(lu[0]) == 'x'){
            gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
            alt_ucs[0] = 's';
            gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
            if(alt_str){
                if(dict_lookup(alt_str) == 1) add_candidate_scored(items, alt_str, 78);
                g_free(alt_str);
            }
            g_free(alt_ucs);
        } else if(llen >= 3 && bare_lower(lu[0]) == 't' && bare_lower(lu[1]) == 'r'){
            gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
            alt_ucs[0] = 'c'; alt_ucs[1] = 'h';
            gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
            if(alt_str){
                if(dict_lookup(alt_str) == 1) add_candidate_scored(items, alt_str, 78);
                g_free(alt_str);
            }
            g_free(alt_ucs);
        } else if(llen >= 3 && bare_lower(lu[0]) == 'c' && bare_lower(lu[1]) == 'h'){
            gunichar *alt_ucs = g_memdup2(lu, llen * sizeof(gunichar));
            alt_ucs[0] = 't'; alt_ucs[1] = 'r';
            gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen, NULL, NULL, NULL);
            if(alt_str){
                if(dict_lookup(alt_str) == 1) add_candidate_scored(items, alt_str, 78);
                g_free(alt_str);
            }
            g_free(alt_ucs);
        } else if(bare_lower(lu[0]) == 'd'){
            /* d -> gi */
            gunichar *alt_ucs = g_new(gunichar, llen + 1);
            alt_ucs[0] = 'g'; alt_ucs[1] = 'i';
            for(glong i=1; i<llen; i++) alt_ucs[i+1] = lu[i];
            gchar *alt_str = g_ucs4_to_utf8(alt_ucs, llen + 1, NULL, NULL, NULL);
            if(alt_str){
                if(dict_lookup(alt_str) == 1) add_candidate_scored(items, alt_str, 75);
                g_free(alt_str);
            }
            g_free(alt_ucs);
        }
    }

    /* 6. Tìm kiếm trong từ điển theo khoảng cách Levenshtein */
    for(int i=0; viet_dict_true[i] && items->len < 10; i++){
        glong dlen = 0;
        gunichar *du = g_utf8_to_ucs4(viet_dict_true[i], -1, NULL, &dlen, NULL);
        if(!du) continue;
        glong diff = llen > dlen ? llen - dlen : dlen - llen;
        if(diff <= 2){
            int dist = lev_ucs4(lu, llen, du, dlen);
            if(dist == 1){
                add_candidate_scored(items, viet_dict_true[i], 70);
            } else if(dist == 2 && items->len < 6){
                add_candidate_scored(items, viet_dict_true[i], 50);
            }
        }
        g_free(du);
    }

    /* 7. Sắp xếp theo điểm và trích xuất top 5 với định dạng chữ hoa/thường nguyên bản */
    g_array_sort(items, compare_scored_cand);
    for(guint i=0; i<items->len && out->len < 5; i++){
        ScoredCand *ci = &g_array_index(items, ScoredCand, i);
        gchar *cased = apply_casing(utf8, ci->word);
        add_candidate_unique(out, cased);
        g_free(cased);
    }

    /* Dọn dẹp */
    for(guint i=0; i<items->len; i++){
        ScoredCand *ci = &g_array_index(items, ScoredCand, i);
        g_free(ci->word);
    }
    g_array_unref(items);
    g_free(lu);
    g_free(lower);

    return out;
}
