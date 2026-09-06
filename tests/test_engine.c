#include "../engine/internal.h"

/* Strict Vietnamese mode: no English restoration; all repeated marks append one raw key. */
static void test_telex(void) {
 const gchar *cases[][2] = {
  {"as", "á"},
  {"af", "à"},
  {"ar", "ả"},
  {"ax", "ã"},
  {"aj", "ạ"},
  {"hienr", "hiển"},
  {"hienj", "hiện"},
  {"vietj", "việt"},
  {"muons", "muốn"},
  {"cuocj", "cuộc"},
  {"tieur", "tiểu"},
  {"chuois", "chuối"},
  {"dd", "đ"},
  {"ddd", "dd"},
  {"reddit", "reddit"},
  {"aa", "â"},
  {"aaa", "aa"},
  {"ee", "ê"},
  {"eee", "ee"},
  {"meet", "mêt"},
  {"meeet", "meet"},
  {"oo", "ô"},
  {"ooo", "oo"},
  {"book", "bôk"},
  {"boook", "book"},
  {"w", "ư"},
  {"ww", "uw"},
  {"www", "uww"},
  {"wwww", "uwww"},
  {"uw", "ư"},
  {"ow", "ơ"},
  {"oww", "ow"},
  {"showw", "showw"},
  {"aw", "ă"},
  {"aww", "aw"},
  {"raww", "raw"},
  {"sw", "sư"},
  {"sww", "suw"},
  {"pass", "pas"},
  {"buff", "buf"},
  {"kiss", "kis"},
  {"boss", "bos"},
  {"toanss", "toans"},
  {"phaps", "pháp"},
  {"phasp", "pháp"},
  {"toans", "toán"},
  {"toasn", "toán"},
  {"hoacw", "hoăc"},
  {"hoacwj", "hoặc"},
  {"password", "pasword"},
  {"passw", "pasw"},
  {"class", "class"},
  {"test", "tét"},
  {"post", "pót"},
  {"fast", "fast"},
  {"fish", "fish"},
  {"code", "code"},
  {"game", "game"},
  {"[", "ươ"},
  {"[[", "["},
  {"]", "ư"},
  {"]]", "]"},
  {"duocwjd", "được"},
  {"duocjwd", "được"},
 };
 for (guint i=0;i<G_N_ELEMENTS(cases);i++) {
  gchar *got=gtv_transform(cases[i][0], GTV_TELEX, TRUE);
  g_assert_cmpstr(got, ==, cases[i][1]); g_free(got);
 }
}
static void test_vni(void) {
 const gchar *cases[][2] = {
  {"a1", "á"},
  {"a2", "à"},
  {"a3", "ả"},
  {"a4", "ã"},
  {"a5", "ạ"},
  {"a6", "â"},
  {"e6", "ê"},
  {"o6", "ô"},
  {"o7", "ơ"},
  {"u7", "ư"},
  {"uo7", "ươ"},
  {"a8", "ă"},
  {"d9", "đ"},
  {"hien2", "hiền"},
  {"viet5", "việt"},
  {"muon1", "muốn"},
  {"tieu3", "tiểu"},
  {"chuoi1", "chuối"},
  {"d9uoc75", "được"},
  {"duoc579", "được"},
  {"duoc795", "được"},
  {"d9uong72", "đường"},
  {"d99", "d9"},
  {"a88", "a8"},
  {"o77", "o7"},
  {"u77", "u7"},
  {"uo77", "uo7"},
  {"a66", "a6"},
  {"e66", "e6"},
  {"o66", "o6"},
  {"a11", "a1"},
  {"ban11", "ban1"},
  {"toan1", "toán"},
  {"toa1n", "toán"},
  {"phap1", "pháp"},
  {"pha1p", "pháp"},
  {"hoac85", "hoặc"},
  {"win10", "win10"},
  {"password1", "password1"},
  {"test1", "test1"},
  {"fast8", "fast8"},
  {"a10", "a"},
  {"toan10", "toan"},
 };
 for (guint i=0;i<G_N_ELEMENTS(cases);i++) {
  gchar *got=gtv_transform(cases[i][0], GTV_VNI, TRUE);
  g_assert_cmpstr(got, ==, cases[i][1]); g_free(got);
 }
}
static void TestTelexCircumflexPreservesCaseAndTone(void) {
 const struct { const gchar *input; gunichar key; const gchar *want; } cases[] = {
  {"á", 'a', "ấ"},
  {"é", 'e', "ế"},
  {"ó", 'o', "ố"},
  {"Á", 'A', "Ấ"},
  {"É", 'E', "Ế"},
  {"Ó", 'O', "Ố"},
  {"ấ", 'a', "áa"},
  {"ế", 'e', "ée"},
  {"ố", 'o', "óo"},
  {"Ấ", 'a', "Áa"},
  {"Ế", 'E', "ÉE"},
  {"Ố", 'O', "ÓO"},
 };
 for(int modern=0; modern<2; modern++) for(guint i=0;i<G_N_ELEMENTS(cases);i++) {
  GString *str=g_string_new(cases[i].input); GArray *buf=gstring_to_ucs4(str);
  g_assert_true(telex_transform(buf,cases[i].key,modern)); ucs4_to_gstring(buf,str);
  g_assert_cmpstr(str->str,==,cases[i].want); g_array_unref(buf);g_string_free(str,TRUE);
 }
}
static void TestAutoPromoteDiphthongPreservesMarks(void) {
 const gchar *cases[][2] = {
  {"hien", "hiên"},
  {"chuyen", "chuyên"},
  {"muon", "muôn"},
  {"tieu", "tiêu"},
  {"yeu", "yêu"},
  {"chuoi", "chuôi"},
  {"HIÉN", "HIẾN"},
  {"MUÓN", "MUỐN"},
  {"TIÉU", "TIẾU"},
  {"uơn", "uơn"},
  {"ưon", "ưon"},
  {"uơi", "uơi"},
  {"ưoi", "ưoi"},
  {"ie", "ie"},
  {"uo", "uo"},
  {"", ""},
 };
 for(guint i=0;i<G_N_ELEMENTS(cases);i++) {
  GString *str=g_string_new(cases[i][0]); GArray *buf=gstring_to_ucs4(str);
  auto_promote_diphthong(buf); ucs4_to_gstring(buf,str);
  g_assert_cmpstr(str->str,==,cases[i][1]);g_array_unref(buf);g_string_free(str,TRUE);
 }
}
static void TestTelexMarkedVowelBeforeFinalConsonant(void) {
 const gchar *cases[][2] = {
  {"bawst", "bắt"},
  {"bawts", "bắt"},
  {"BAWST", "BẮT"},
  {"caast", "cất"},
  {"coost", "cốt"},
  {"bowst", "bớt"},
  {"test", "tét"},
  {"post", "pót"},
  {"fast", "fast"},
  {"fish", "fish"},
  {"task", "ták"},
 };
 for(int modern=0;modern<2;modern++) for(guint i=0;i<G_N_ELEMENTS(cases);i++) {
  gchar *got=gtv_transform(cases[i][0],GTV_TELEX,modern);
  g_assert_cmpstr(got,==,cases[i][1]);g_free(got);
 }
}
static void TestTelexLateHornAndStroke(void) {
 const gchar *cases[][2] = {
  {"duocjwd", "được"},
  {"duocwjd", "được"},
  {"dduocjw", "được"},
  {"duowcjd", "được"},
  {"duongwf", "dường"},
  {"duocjd", "đuộc"},
  {"dand", "đan"},
  {"ddd", "dd"},
  {"reddit", "reddit"},
  {"password", "pasword"},
 };
 for(int modern=0;modern<2;modern++) for(guint i=0;i<G_N_ELEMENTS(cases);i++) {
  gchar *got=gtv_transform(cases[i][0],GTV_TELEX,modern);
  g_assert_cmpstr(got,==,cases[i][1]);g_free(got);
 }
}
int main(int argc,char **argv) {
 g_test_init(&argc,&argv,NULL); gtv_init();
 g_test_add_func("/engine/telex",test_telex);
 g_test_add_func("/engine/vni",test_vni);
 g_test_add_func("/engine/TestTelexCircumflexPreservesCaseAndTone",TestTelexCircumflexPreservesCaseAndTone);
 g_test_add_func("/engine/TestAutoPromoteDiphthongPreservesMarks",TestAutoPromoteDiphthongPreservesMarks);
 g_test_add_func("/engine/TestTelexMarkedVowelBeforeFinalConsonant",TestTelexMarkedVowelBeforeFinalConsonant);
 g_test_add_func("/engine/TestTelexLateHornAndStroke",TestTelexLateHornAndStroke);
 return g_test_run();
}
