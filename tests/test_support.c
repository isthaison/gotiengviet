#include "../engine/internal.h"
#include <glib/gstdio.h>

static void test_stateful(void) {
    GtvConfig config = {.mode=GTV_TELEX,.modern=TRUE,.spellcheck=TRUE};
    GtvEngine *engine=gtv_engine_new(&config);
    const gchar *input="duocjwd";
    for(const gchar *p=input;*p;p++) g_assert_null(gtv_engine_process(engine,*p,NULL));
    gchar *buffer=gtv_engine_buffer(engine);
    g_assert_cmpstr(buffer,==,"được"); g_free(buffer);
    guint backspaces;
    g_assert_null(gtv_engine_process(engine,'\b',&backspaces));
    g_assert_cmpuint(backspaces,==,1);
    gchar *commit=gtv_engine_process(engine,' ',NULL);
    g_assert_cmpstr(commit,==,"đượ ");g_free(commit);
    const gchar *phrases[]={"vn ",":SMILE: ","ko!",NULL};
    const gchar *expected[]={"Việt Nam ","😊 ","không!"};
    for(guint i=0;phrases[i];i++) {
        for(const gchar *p=phrases[i];*p;p++) {
            commit=gtv_engine_process(engine,*p,NULL);
            if(commit) {g_assert_cmpstr(commit,==,expected[i]);g_free(commit);}
        }
    }
    gtv_engine_process(engine,'[',NULL);
    buffer=gtv_engine_buffer(engine);g_assert_cmpstr(buffer,==,"ươ");g_free(buffer);
    gtv_engine_process(engine,'[',NULL);
    buffer=gtv_engine_buffer(engine);g_assert_cmpstr(buffer,==,"[");g_free(buffer);
    gtv_engine_reset(engine);
    buffer=gtv_engine_buffer(engine);g_assert_cmpstr(buffer,==,"");g_free(buffer);
    gtv_engine_free(engine);
    g_assert_null(gtv_transform("\xff",GTV_TELEX,TRUE));
}
static void test_config(void) {
    gchar *directory=g_dir_make_tmp("gotiengviet-config-XXXXXX",NULL);
    g_assert_nonnull(directory);
    GtvConfig config,loaded;
    gtv_config_load(&config,directory);
    g_assert_cmpint(config.mode,==,GTV_TELEX);
    g_assert_true(config.modern);g_assert_true(config.spellcheck);
    g_assert_false(config.ai_enabled);
    config.mode=GTV_VNI;config.modern=FALSE;config.spellcheck=FALSE;config.ai_enabled=TRUE;
    g_free(config.model);config.model=g_strdup("local-model");
    g_assert_true(gtv_config_save(&config,directory,NULL));
    gtv_config_load(&loaded,directory);
    g_assert_cmpint(loaded.mode,==,GTV_VNI);g_assert_false(loaded.modern);g_assert_false(loaded.spellcheck);
    g_assert_true(loaded.ai_enabled);g_assert_cmpstr(loaded.model,==,"local-model");
    gtv_config_clear(&loaded);gtv_config_clear(&config);
    gchar *path=g_build_filename(directory,"ai.conf",NULL);
    g_assert_true(g_file_set_contents(path,"[ai]\nprovider=rule\nmodel=override\n",-1,NULL));
    gtv_config_load(&loaded,directory);
    g_assert_false(loaded.ai_enabled);g_assert_cmpstr(loaded.model,==,"override");
    gtv_config_clear(&loaded);
    g_remove(path);g_free(path);
    path=g_build_filename(directory,"config",NULL);g_remove(path);g_free(path);
    g_rmdir(directory);g_free(directory);
}
static void test_json(void) {
    const gchar *valid[]={"{\"response\":\"được\"}","{\"x\":[1, true, null, {\"response\":\"ignore\"}],\"response\":\"\\u0111\\u01b0\\u1ee3c\"}","{\"response\":\"\\ud83d\\ude0a\"}",NULL};
    const gchar *expected[]={"được","được","😊"};
    for(guint i=0;valid[i];i++) {gchar *got=gtv_json_response(valid[i]);g_assert_cmpstr(got,==,expected[i]);g_free(got);}
    const gchar *invalid[]={"", "{", "{\"response\":\"x\",}","{\"response\":\"x\"}junk","{\"response\":\"\\ud800\"}","{\"response\":\"\\u0000\"}","{\"response\":false}","{\"nested\":{\"response\":\"x\"}}",NULL};
    for(guint i=0;invalid[i];i++) g_assert_null(gtv_json_response(invalid[i]));
    gchar *quoted=gtv_json_quote("a\"\\\nđ");
    gchar *object=g_strdup_printf("{\"response\":%s}",quoted);
    gchar *decoded=gtv_json_response(object);
    g_assert_cmpstr(decoded,==,"a\"\\\nđ");g_free(decoded);g_free(object);g_free(quoted);
}
static void test_spelling(void) {
    g_assert_true(spell_word_valid("được"));
    g_assert_false(spell_word_valid("kông"));
    GtvConfig config={.ai_enabled=FALSE};
    g_assert_true(gtv_ai_available(&config));
    GPtrArray *suggestions=gtv_ai_suggest(&config,"kông","");
    g_assert_cmpuint(suggestions->len,>,0);g_ptr_array_unref(suggestions);
    suggestions=gtv_ai_suggest(&config,"được","");
    g_assert_cmpuint(suggestions->len,==,0);g_ptr_array_unref(suggestions);
}

/* Every permutation of pending operations must converge to the same syllable. */
static void permute(const gchar *prefix, gchar *keys, guint index, GtvMode mode, gboolean modern, const gchar *want) {
    if(!keys[index]) {
        gchar *input=g_strconcat(prefix,keys,NULL);
        gchar *got=gtv_transform(input,mode,modern);
        if(g_strcmp0(got,want)) g_error("%s (mode=%d,modern=%d) -> %s, expected %s",input,mode,modern,got,want);
        g_free(got);g_free(input);return;
    }
    for(guint i=index;keys[i];i++) {
        gchar swap=keys[index];keys[index]=keys[i];keys[i]=swap;
        permute(prefix,keys,index+1,mode,modern,want);
        swap=keys[index];keys[index]=keys[i];keys[i]=swap;
    }
}
static void test_order_independence(void) {
    const gchar tones[]="sfrxj";
    for(int modern=0;modern<2;modern++) for(int upper=0;upper<2;upper++) for(int tone=1;tone<=5;tone++) {
        gunichar vowel;g_assert_true(lookup_char('o',DIAC_HORN,tone,upper,&vowel));
        GString *want=g_string_new(upper?"ĐƯ":"đư");g_string_append_unichar(want,vowel);g_string_append(want,upper?"C":"c");
        gchar telex[]={upper?'C':'c',upper?'W':'w',upper?'D':'d',upper?g_ascii_toupper(tones[tone-1]):tones[tone-1],0};
        gchar vni[]={upper?'C':'c','7','9',(gchar)('0'+tone),0};
        permute(upper?"DUO":"duo",telex,0,GTV_TELEX,modern,want->str);
        permute(upper?"DUO":"duo",vni,0,GTV_VNI,modern,want->str);
        g_string_free(want,TRUE);
    }
    gchar telex[]="wst",vni[]="81t";
    permute("ba",telex,0,GTV_TELEX,TRUE,"bắt");
    permute("ba",vni,0,GTV_VNI,TRUE,"bắt");
}
static void test_random_input(void) {
    GRand *rng=g_rand_new_with_seed(95);
    const gchar *alphabet="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]{}:";
    for(int mode=0;mode<2;mode++) for(int n=0;n<10000;n++) {
        gchar input[65];
        guint length=g_rand_int_range(rng,0,65);
        for(guint i=0;i<length;i++) input[i]=alphabet[g_rand_int_range(rng,0,strlen(alphabet))];
        input[length]=0;
        gchar *out=gtv_transform(input,mode,n%2);
        g_assert_nonnull(out);g_assert_true(g_utf8_validate(out,-1,NULL));
        g_assert_cmpint(g_utf8_strlen(out,-1),<=,length*2);
        g_free(out);
    }
    g_rand_free(rng);
}
static void test_ollama(void) {
    GtvConfig config={.ai_enabled=TRUE,.url="http://localhost:55602",.model="qwen2:0.5b"};
    g_assert_true(gtv_ai_available(&config));
    GPtrArray *out=gtv_ai_suggest(&config,"kông","a\"b");
    g_assert_cmpuint(out->len,==,1);g_assert_cmpstr(g_ptr_array_index(out,0),==,"không");g_ptr_array_unref(out);
    config.url="http://malformed";
    out=gtv_ai_suggest(&config,"kông","a\"b");
    g_assert_cmpuint(out->len,>,0);g_ptr_array_unref(out);
    config.url="http://fail";
    g_assert_false(gtv_ai_available(&config));
    out=gtv_ai_suggest(&config,"được","");g_assert_cmpuint(out->len,==,0);g_ptr_array_unref(out);

}
int main(int argc,char **argv) {
    const gchar *fixture=g_getenv("GTV_TEST_CURL_DIR");
    g_assert_nonnull(fixture);
    gchar *path=g_strconcat(fixture,":",g_getenv("PATH")?g_getenv("PATH"):"",NULL);
    g_setenv("PATH",path,TRUE);g_free(path);
    g_test_init(&argc,&argv,NULL);gtv_init();
    g_test_add_func("/support/stateful",test_stateful);
    g_test_add_func("/support/config",test_config);
    g_test_add_func("/support/json",test_json);
    g_test_add_func("/support/ollama",test_ollama);
    g_test_add_func("/support/spelling",test_spelling);
    g_test_add_func("/algorithm/order-independence",test_order_independence);
    g_test_add_func("/algorithm/random-input",test_random_input);
    return g_test_run();
}
