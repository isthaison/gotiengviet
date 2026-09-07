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

void add_candidate_unique(GPtrArray *out, const char *cand){
    if(!cand || !*cand) return;
    for(guint k=0; k<out->len; k++){
        if(strcmp((char*)out->pdata[k], cand) == 0) return;
    }
    g_ptr_array_add(out, g_strdup(cand));
}

/* Gợi ý từ/cụm từ do Ollama xử lý (engine/ai.c, bất đồng bộ, có hủy).
 * Kiểm tra đúng/sai ngoại tuyến vẫn dùng spell_word_valid() ở trên để
 * không chặn đường xử lý phím. */
