#include "internal.h"

static const char *macro_pairs[][2] = {
    {"vn","Việt Nam"}, {"hn","Hà Nội"}, {"hcm","Hồ Chí Minh"},
    {"dc","được"}, {"ko","không"}, {"ntn","như thế nào"},
    {"cx","cũng"}, {"cb","chuẩn bị"},
    {NULL, NULL}
};
static const char *emoji_pairs[][2] = {
    /* Faces & Emotions */
    {":smile:", "😊"}, {":smiley:", "😃"}, {":grin:", "😁"},
    {":laugh:", "😂"}, {":joy:", "😂"}, {":rofl:", "🤣"},
    {":heart_eyes:", "😍"}, {":love:", "😍"}, {":kiss:", "😘"},
    {":wink:", "😉"}, {":blush:", "😊"}, {":sunglasses:", "😎"}, {":cool:", "😎"},
    {":star_struck:", "🤩"}, {":thinking:", "🤔"}, {":smirk:", "😏"},
    {":sweat:", "😅"}, {":sad:", "😢"}, {":cry:", "😭"}, {":sob:", "😭"},
    {":angry:", "😠"}, {":rage:", "😡"}, {":scream:", "😱"}, {":shock:", "😱"},
    {":neutral:", "😐"}, {":relieved:", "😌"}, {":sleeping:", "😴"},
    {":clown:", "🤡"}, {":ghost:", "👻"}, {":skull:", "💀"}, {":poop:", "💩"},
    {":yum:", "😋"}, {":tongue:", "😛"},

    /* Gestures & People */
    {":thumbsup:", "👍"}, {":+1:", "👍"}, {":like:", "👍"},
    {":thumbsdown:", "👎"}, {":-1:", "👎"}, {":dislike:", "👎"},
    {":clap:", "👏"}, {":pray:", "🙏"}, {":ok_hand:", "👌"}, {":ok:", "👌"},
    {":wave:", "👋"}, {":muscle:", "💪"}, {":handshake:", "🤝"},
    {":punch:", "👊"}, {":heart_hands:", "🫶"}, {":v:", "✌️"}, {":peace:", "✌️"},

    /* Hearts & Symbols */
    {":heart:", "❤️"}, {":blue_heart:", "💙"}, {":green_heart:", "💚"},
    {":yellow_heart:", "💛"}, {":purple_heart:", "💜"}, {":sparkling_heart:", "💖"},
    {":broken_heart:", "💔"}, {":fire:", "🔥"}, {":sparkles:", "✨"},
    {":star:", "⭐"}, {":star2:", "🌟"}, {":tada:", "🎉"}, {":party:", "🎉"},
    {":rocket:", "🚀"}, {":100:", "💯"}, {":check:", "✅"}, {":cross:", "❌"},
    {":warning:", "⚠️"}, {":question:", "❓"}, {":exclamation:", "❗️"},

    /* Food, Animals, Objects & Places */
    {":coffee:", "☕"}, {":beer:", "🍺"}, {":pizza:", "🍕"}, {":cake:", "🎂"},
    {":sun:", "☀️"}, {":moon:", "🌙"}, {":cat:", "🐱"}, {":dog:", "🐶"},
    {":car:", "🚗"}, {":airplane:", "✈️"}, {":bulb:", "💡"}, {":money:", "💰"},
    {":vim:", "💚"}, {":vn:", "🇻🇳"}, {":vietnam:", "🇻🇳"}, {":flag_vn:", "🇻🇳"},

    /* Common Text Emoticons */
    {":)", "😊"}, {":-)", "😊"}, {":]", "😊"},
    {":D", "😀"}, {":-D", "😀"},
    {":(", "😢"}, {":-(", "😢"},
    {";)", "😉"}, {";-)", "😉"},
    {":P", "😛"}, {":-P", "😛"}, {":p", "😛"}, {":-p", "😛"},
    {":3", "🐱"},
    {"<3", "❤️"}, {"</3", "💔"},
    {"(y)", "👍"}, {"(n)", "👎"},
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

GPtrArray* get_emoji_suggestions(const char *prefix){
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if(!prefix || !*prefix) return out;
    gchar *lower = g_utf8_strdown(prefix, -1);
    for(int i = 0; emoji_pairs[i][0]; i++){
        const char *code = emoji_pairs[i][0];
        const char *emoji = emoji_pairs[i][1];
        if(code[0] == ':' && g_str_has_prefix(code, lower)){
            add_candidate_unique(out, emoji);
            if(out->len >= 5) break;
        }
    }
    g_free(lower);
    return out;
}
