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
    const gchar *phrases[]={"vn ",":SMILE:","ko!",NULL};
    const gchar *expected[]={"Việt Nam ","😊","không!"};
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
    g_assert_true(g_file_set_contents(path,"[ai]\nmodel=qwen2:0.5b (~400MB)\n",-1,NULL));
    gtv_config_load(&loaded,directory);
    g_assert_cmpstr(loaded.model,==,"qwen2:0.5b");gtv_config_clear(&loaded);
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
    g_assert_false(spell_word_valid("kông")); /* k chỉ đi với i/e/y */
    /* Thuần quy tắc âm tiết, không danh sách cứng: hoăc/ge hợp cấu trúc nên
     * qua vòng sync; lỗi kiểu này do Ollama sửa và được nhớ vào
     * learned-corrections.txt để gạch đỏ ngay lần sau (xem test_ibus). */
    g_assert_true(spell_word_valid("hoăc"));
    g_assert_false(spell_word_valid("bàc")); /* huyền + phụ âm tắc c */
    g_assert_false(spell_word_valid("vièt")); /* huyền + phụ âm tắc t */
    /* Chưa gõ dấu thì sync luôn cho qua (đang gõ dở/chữ thô); ngh+a sai
     * vẫn do Ollama sửa và được nhớ vào learned-corrections.txt. */
    g_assert_true(spell_word_valid("ngha"));
    g_assert_true(spell_word_valid("ge"));

    GtvConfig config={.ai_enabled=FALSE};
    g_assert_false(gtv_ai_available(&config));
    GPtrArray *suggestions=gtv_ai_suggest(&config,"kông","");
    g_assert_cmpuint(suggestions->len,==,0);
    g_ptr_array_unref(suggestions);

    /* Suggestions no longer capitalize based on the input. */
    suggestions=gtv_ai_suggest(&config,"Kông","");
    g_assert_cmpuint(suggestions->len,==,0);
    g_ptr_array_unref(suggestions);

    suggestions=gtv_ai_suggest(&config,"KÔNG","");
    g_assert_cmpuint(suggestions->len,==,0);
    g_ptr_array_unref(suggestions);

    /* Test voiceless stop correction */
    suggestions=gtv_ai_suggest(&config,"vièt","");
    g_assert_cmpuint(suggestions->len,==,0);
    g_ptr_array_unref(suggestions);

    /* Test hoăc -> hoặc */
    suggestions=gtv_ai_suggest(&config,"hoăc","");
    g_assert_cmpuint(suggestions->len,==,0);
    g_ptr_array_unref(suggestions);

    suggestions=gtv_ai_suggest(&config,"được","");
    g_assert_cmpuint(suggestions->len,==,0);
    g_ptr_array_unref(suggestions);
}

static void test_prompts(void) {
    gchar *tmp=g_dir_make_tmp("gotiengviet-prompts-XXXXXX",NULL);
    g_assert_nonnull(tmp);
    gchar *saved=g_strdup(g_getenv("GTV_DATA_DIR"));
    g_setenv("GTV_DATA_DIR",tmp,TRUE);
    gtv_prompts_reload();
    /* Empty dir: everything falls back. */
    gchar *p=gtv_prompt_get("suggest","prompt","FB");
    g_assert_cmpstr(p,==,"FB");g_free(p);
    g_assert_null(gtv_prompt_get("suggest","prompt",NULL));
    /* Custom file is honored. */
    gchar *pc=g_build_filename(tmp,"prompts.conf",NULL);
    g_assert_true(g_file_set_contents(pc,"[suggest]\nprompt=A:%s:%s:%s\ncorrect_hint=H.\n",-1,NULL));
    gtv_prompts_reload();
    p=gtv_prompt_get("suggest","prompt","FB");
    g_assert_cmpstr(p,==,"A:%s:%s:%s");g_free(p);
    p=gtv_prompt_get("suggest","complete_hint","FB");
    g_assert_cmpstr(p,==,"FB");g_free(p);
    /* Template formatting substitutes %s in order, keeps the rest literally. */
    const gchar *args[]={"x","y"};
    gchar *f=gtv_format_template("a %d %s b %% %s c %s",args,2);
    g_assert_cmpstr(f,==,"a %d x b %% y c %s");g_free(f);
    f=gtv_format_template(NULL,args,2);
    g_assert_cmpstr(f,==,"");g_free(f);
    /* Restore shipped data. */
    if(saved)g_setenv("GTV_DATA_DIR",saved,TRUE);
    g_free(saved);
    gtv_prompts_reload();
    p=gtv_prompt_get("suggest","correct_hint","FB");
    g_assert_cmpstr(p,==,"Correct spelling if needed.");g_free(p);
    g_remove(pc);g_free(pc);
    g_rmdir(tmp);g_free(tmp);
}
static void test_config_defaults(void) {
    /* Shipped data files drive defaults; user files override them. */
    gchar *empty=g_dir_make_tmp("gotiengviet-defaults-XXXXXX",NULL);
    g_assert_nonnull(empty);
    GtvConfig cfg;
    gtv_config_load(&cfg,empty);
    g_assert_cmpint(cfg.mode,==,GTV_TELEX);
    g_assert_true(cfg.modern);g_assert_true(cfg.spellcheck);
    g_assert_false(cfg.ai_enabled);
    g_assert_cmpstr(cfg.model,==,"qwen2:0.5b");
    g_assert_cmpstr(cfg.url,==,"http://localhost:55602");
    g_assert_cmpstr(cfg.port,==,"55602");
    gtv_config_clear(&cfg);
    gchar *uc=g_build_filename(empty,"config",NULL);
    g_assert_true(g_file_set_contents(uc,"[input]\nmethod=vni\n",-1,NULL));
    g_free(uc);
    gtv_config_load(&cfg,empty);
    g_assert_cmpint(cfg.mode,==,GTV_VNI);
    g_assert_cmpstr(cfg.model,==,"qwen2:0.5b");
    gtv_config_clear(&cfg);
    gchar *ua=g_build_filename(empty,"ai.conf",NULL);
    g_assert_true(g_file_set_contents(ua,"[ai]\nprovider=ollama\nmodel=custom-model\n",-1,NULL));
    g_free(ua);
    gtv_config_load(&cfg,empty);
    g_assert_true(cfg.ai_enabled);
    g_assert_cmpstr(cfg.model,==,"custom-model");
    gtv_config_clear(&cfg);
    gchar *rc=g_build_filename(empty,"config",NULL);g_remove(rc);g_free(rc);
    gchar *ra=g_build_filename(empty,"ai.conf",NULL);g_remove(ra);g_free(ra);
    g_rmdir(empty);g_free(empty);
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
    g_assert_cmpuint(out->len,==,0);g_ptr_array_unref(out);
    config.url="http://fail";
    g_assert_false(gtv_ai_available(&config));
    out=gtv_ai_suggest(&config,"được","");g_assert_cmpuint(out->len,==,0);g_ptr_array_unref(out);
}

static void on_async_suggest_done(GObject *src, GAsyncResult *res, gpointer data){
    gboolean *done = data;
    GError *error = NULL;
    GPtrArray *out = gtv_suggest_combined_finish(res, &error);
    g_assert_no_error(error);
    g_assert_nonnull(out);
    g_assert_cmpuint(out->len, >, 0);
    g_ptr_array_unref(out);
    *done = TRUE;
}

static void test_suggest_combined(void) {
    GtvConfig config = {.ai_enabled = TRUE,.url="http://localhost:55602",.model="qwen2:0.5b"};
    GPtrArray *s1 = gtv_suggest_combined(&config, "Tôi ", "kông", TRUE);
    g_assert_nonnull(s1);
    g_assert_cmpuint(s1->len, >, 0);
    g_assert_cmpstr(g_ptr_array_index(s1, 0), ==, "không");
    g_ptr_array_unref(s1);

    GPtrArray *s2 = gtv_suggest_combined(&config, "xin", "ch", FALSE);
    g_assert_nonnull(s2);
    g_assert_cmpuint(s2->len, >, 0);
    g_assert_cmpstr(g_ptr_array_index(s2, 0), ==, "chào");
    g_ptr_array_unref(s2);

    gboolean done = FALSE;
    gtv_suggest_combined_async(&config, "xin", "ch", FALSE, NULL, on_async_suggest_done, &done);
    while(!done) {
        g_main_context_iteration(NULL, TRUE);
    }
    g_assert_true(done);
}

static void test_macro_and_emoji(void) {
    /* Test expand_word for emojis */
    gchar *e1 = expand_word(":smile:");
    g_assert_cmpstr(e1, ==, "😊");
    g_free(e1);

    gchar *e2 = expand_word(":heart:");
    g_assert_cmpstr(e2, ==, "❤️");
    g_free(e2);

    gchar *e3 = expand_word(":fire:");
    g_assert_cmpstr(e3, ==, "🔥");
    g_free(e3);

    /* Test expand_word for text emoticons */
    gchar *m1 = expand_word(":)");
    g_assert_cmpstr(m1, ==, "😊");
    g_free(m1);

    gchar *m2 = expand_word("<3");
    g_assert_cmpstr(m2, ==, "❤️");
    g_free(m2);

    /* Test macro expansion */
    gchar *mc1 = expand_word("vn");
    g_assert_cmpstr(mc1, ==, "Việt Nam");
    g_free(mc1);

    gchar *mc2 = expand_word("dc");
    g_assert_cmpstr(mc2, ==, "được");
    g_free(mc2);

    /* Test get_emoji_suggestions */
    GPtrArray *sugs = get_emoji_suggestions(":sm");
    g_assert_nonnull(sugs);
    g_assert_cmpuint(sugs->len, >, 0);
    g_assert_cmpstr(g_ptr_array_index(sugs, 0), ==, "😊");
    g_ptr_array_unref(sugs);

    /* Test instant commit through gtv_engine_process */
    GtvConfig cfg = {.mode = GTV_TELEX, .modern = TRUE};
    GtvEngine *eng = gtv_engine_new(&cfg);

    const char *seq = ":smile:";
    gchar *commit = NULL;
    for(const char *p = seq; *p; p++){
        g_free(commit);
        commit = gtv_engine_process(eng, (gunichar)*p, NULL);
    }
    g_assert_nonnull(commit);
    g_assert_cmpstr(commit, ==, "😊");
    g_free(commit);

    /* Test instant commit for :) */
    gtv_engine_reset(eng);
    commit = gtv_engine_process(eng, ':', NULL);
    g_assert_null(commit);
    commit = gtv_engine_process(eng, ')', NULL);
    g_assert_nonnull(commit);
    g_assert_cmpstr(commit, ==, "😊");
    g_free(commit);

    /* Test instant commit for :-) */
    gtv_engine_reset(eng);
    g_assert_null(gtv_engine_process(eng, ':', NULL));
    g_assert_null(gtv_engine_process(eng, '-', NULL));
    commit = gtv_engine_process(eng, ')', NULL);
    g_assert_nonnull(commit);
    g_assert_cmpstr(commit, ==, "😊");
    g_free(commit);

    /* Test instant commit for <3 */
    gtv_engine_reset(eng);
    g_assert_null(gtv_engine_process(eng, '<', NULL));
    commit = gtv_engine_process(eng, '3', NULL);
    g_assert_nonnull(commit);
    g_assert_cmpstr(commit, ==, "❤️");
    g_free(commit);

    /* Test instant commit for :D */
    gtv_engine_reset(eng);
    g_assert_null(gtv_engine_process(eng, ':', NULL));
    commit = gtv_engine_process(eng, 'D', NULL);
    g_assert_nonnull(commit);
    g_assert_cmpstr(commit, ==, "😀");
    g_free(commit);

    /* Test normal colon after word */
    gtv_engine_reset(eng);
    for(const char *p = "chao"; *p; p++) g_assert_null(gtv_engine_process(eng, *p, NULL));
    commit = gtv_engine_process(eng, ':', NULL);
    g_assert_nonnull(commit);
    g_assert_cmpstr(commit, ==, "chao:");
    g_free(commit);

    gtv_engine_free(eng);
}

int main(int argc,char **argv) {
    const gchar *fixture=g_getenv("GTV_TEST_CURL_DIR");
    g_assert_nonnull(fixture);
    gchar *path=g_strconcat(fixture,G_SEARCHPATH_SEPARATOR_S,g_getenv("PATH")?g_getenv("PATH"):"",NULL);
    g_setenv("PATH",path,TRUE);g_free(path);
    g_test_init(&argc,&argv,NULL);gtv_init();
    g_test_add_func("/support/stateful",test_stateful);
    g_test_add_func("/support/config",test_config);
    g_test_add_func("/support/json",test_json);
    g_test_add_func("/support/ollama",test_ollama);
    g_test_add_func("/support/suggest-combined",test_suggest_combined);
    g_test_add_func("/support/spelling",test_spelling);
    g_test_add_func("/support/macro-and-emoji",test_macro_and_emoji);
    g_test_add_func("/support/prompts",test_prompts);
    g_test_add_func("/support/config-defaults",test_config_defaults);
    g_test_add_func("/algorithm/order-independence",test_order_independence);
    g_test_add_func("/algorithm/random-input",test_random_input);
    return g_test_run();
}
