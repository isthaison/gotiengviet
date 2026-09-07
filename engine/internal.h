#ifndef GTV_INTERNAL_H
#define GTV_INTERNAL_H
#include "engine.h"
#include <string.h>
#include <stdio.h>

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

gchar* reverse_key(gunichar bare, int dia, int tone, gboolean upper);
void register_char(gunichar ch, gunichar bare, int dia, int tone, gboolean upper);
void init_charset(void);
CharInfo* get_info(gunichar c);
gboolean lookup_char(gunichar bare, int dia, int tone, gboolean upper, gunichar *out);
gboolean is_vowel(gunichar c);
gunichar to_lower_g(gunichar c);
gint get_tone(gunichar c);
gint get_diac(gunichar c);
gunichar bare_lower(gunichar c);
int find_tone_position(GArray *word, gboolean modern);
void remove_all_tones(GArray *word);
gboolean apply_tone_at(GArray *word, int pos, int tone);
int dict_lookup(const char *lower_utf8);
gboolean is_valid_coda(const char *coda, int coda_len);
gboolean is_valid_onset(const gunichar *ucs, glong first_v);
gboolean spell_word_valid(const char *utf8);
int lev_ucs4(const gunichar *a, glong la, const gunichar *b, glong lb);
void add_candidate_unique(GPtrArray *out, const char *cand);
GPtrArray* get_suggestions(const char *utf8);
void auto_promote_diphthong(GArray *buf);
gboolean telex_transform(GArray *buf, gunichar key, gboolean modern);
gboolean vni_transform(GArray *buf, gunichar key, gboolean modern);
GArray* gstring_to_ucs4(GString *s);
void ucs4_to_gstring(GArray *arr, GString *s);
gchar* expand_word(const char *word);
GPtrArray* get_emoji_suggestions(const char *prefix);

gchar *gtv_json_quote(const gchar *text);
gchar *gtv_json_response(const gchar *json);
gboolean gtv_apply_key(GArray *buf, gunichar key, GtvMode mode, gboolean modern);
void gtv_compose(GArray *buf, gunichar key, GtvMode mode, gboolean modern);

GPtrArray *gtv_suggest_combined(const GtvConfig *config, const gchar *context, const gchar *preedit, gboolean bad);
void gtv_suggest_combined_async(const GtvConfig *config, const gchar *context, const gchar *preedit, gboolean bad,
                               GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);
GPtrArray *gtv_suggest_combined_finish(GAsyncResult *res, GError **error);
#endif
