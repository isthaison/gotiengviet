#include "internal.h"

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
gchar* expand_word(const char *word){
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
