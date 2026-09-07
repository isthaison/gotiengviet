#define main ibus_application_main
#include "../ibus/engine.c"
#undef main
#include <glib/gstdio.h>

static GString *application_text;
static guint application_cursor, replacement_signals;
static gboolean application_backspace(gpointer data){
    g_assert_cmpuint(application_cursor,>,0);
    gchar *end=g_utf8_offset_to_pointer(application_text->str,application_cursor);
    gchar *start=g_utf8_prev_char(end);
    g_string_erase(application_text,start-application_text->str,end-start);application_cursor--;
    replacement_signals++;return G_SOURCE_REMOVE;
}
static void application_signal(GDBusConnection *connection,const gchar *sender,const gchar *path,
                               const gchar *interface,const gchar *name,GVariant *parameters,gpointer data){
    if(!strcmp(name,"DeleteSurroundingText")){
        gint offset;guint length;g_variant_get(parameters,"(iu)",&offset,&length);
        guint start=application_cursor+offset;
        gchar *from=g_utf8_offset_to_pointer(application_text->str,start);
        gchar *end=g_utf8_offset_to_pointer(from,length);
        g_string_erase(application_text,from-application_text->str,end-from);application_cursor=start;
        replacement_signals++;
    }else if(!strcmp(name,"ForwardKeyEvent")){
        guint key,code,state;g_variant_get(parameters,"(uuu)",&key,&code,&state);
        g_assert_cmpuint(key,==,IBUS_BackSpace);
        g_assert_cmpuint(code,==,KEY_BACKSPACE);
        /* Model a client which queues native key events instead of applying
         * them synchronously inside the IBus signal callback. */
        if(!(state & IBUS_RELEASE_MASK))g_timeout_add(5,application_backspace,NULL);
        else replacement_signals++;
    }else if(!strcmp(name,"CommitText")){
        GVariant *serialized=NULL;g_variant_get(parameters,"(v)",&serialized);
        IBusText *text=IBUS_TEXT(ibus_serializable_deserialize(serialized));g_object_ref_sink(text);
        gchar *at=g_utf8_offset_to_pointer(application_text->str,application_cursor);
        g_string_insert(application_text,at-application_text->str,text->text);
        application_cursor+=g_utf8_strlen(text->text,-1);replacement_signals++;
        g_object_unref(text);g_variant_unref(serialized);
    }
}
static GtvTextTarget *no_accessible_target(const gchar *a,const gchar *b){return NULL;}
int main(int argc,char **argv) {
    gchar *test_path=g_strconcat(g_getenv("GTV_TEST_CURL_DIR"),G_SEARCHPATH_SEPARATOR_S,g_getenv("PATH"),NULL);
    g_setenv("PATH",test_path,TRUE);g_free(test_path);
    g_test_init(&argc,&argv,NULL);
    select_text_target=no_accessible_target;
    gchar *directory=g_dir_make_tmp("gotiengviet-ibus-test-XXXXXX",NULL);
    g_assert_nonnull(directory);
    g_setenv("XDG_CONFIG_HOME",directory,TRUE);
    gchar *config_dir=g_build_filename(directory,"gotiengviet",NULL);
    GtvConfig config;gtv_config_load(&config,config_dir);config.spellcheck=FALSE;
    g_assert_true(gtv_config_save(&config,config_dir,NULL));gtv_config_clear(&config);
    GTestDBus *test_bus=g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(test_bus);
    GError *error=NULL;
    GDBusConnection *connection=g_dbus_connection_new_for_address_sync(g_test_dbus_get_bus_address(test_bus),
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,NULL,NULL,&error);
    g_assert_no_error(error);gtv_init();ibus_init();
    IBusEngine *engine=g_object_new(ibus_gotiengviet_engine_get_type(),"engine-name","gotiengviet",
        "object-path","/org/freedesktop/IBus/Engine/Test","connection",connection,"has-focus-id",TRUE,NULL);
    g_object_ref_sink(engine);
    IBusGoTiengVietEngine *e=(IBusGoTiengVietEngine*)engine;
    ibus_gotiengviet_engine_focus_in(engine);
    const struct {const gchar *input,*want;gboolean telex;} cases[]={
        {"bawst","bắt",TRUE},{"duocjwd","được",TRUE},{"DUOCJWD","ĐƯỢC",TRUE},
        {"ass","as",TRUE},{"duoc579","được",FALSE},{"[[","[",TRUE},{":smile:","",TRUE}
    };
    for(guint i=0;i<G_N_ELEMENTS(cases);i++) {
        ibus_gotiengviet_engine_reset(e);e->mode_telex=cases[i].telex;
        for(const gchar *p=cases[i].input;*p;p++) {
            g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,(guint)*p,0,0));
            if(e->n_candidates > 1) {
                g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_Down,0,0));
                g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_Up,0,0));
            }
        }
        g_assert_cmpstr(e->preedit->str,==,cases[i].want);
    }
    g_assert_cmpstr(e->sentence_context->str,==,"😊");
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_a,0,0));
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_space,0,0));
    g_assert_cmpstr(e->preedit->str,==,"");
    /* Partial syllables can be marked misspelled but must retain contextual completions. */
    ibus_gotiengviet_engine_reset(e);
    e->spellcheck=TRUE;e->mode_telex=TRUE;e->config.ai_enabled=TRUE;
    g_string_assign(e->sentence_context,"xin");
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_c,0,0));
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_h,0,0));
    g_assert_cmpint(e->n_candidates,==,0);
    g_assert_cmpuint(e->suggest_timer,>,0);g_assert_null(e->ai_cancellable);
    for(guint i=0;i<150 && !e->n_candidates;i++){while(g_main_context_iteration(NULL,FALSE));g_usleep(10000);}
    g_assert_cmpint(e->n_candidates,>,0);
    g_assert_cmpstr(e->candidates[0],==,"chào");
    e->config.ai_enabled=FALSE;
    g_assert_cmpint(e->n_candidates,<=,5);
    /* Only explicitly accepted AI candidates are persisted. */
    g_assert_false(word_valid(e,"kông"));
    e->candidates_from_ai=FALSE;learn_candidate(e,"kông");
    g_assert_false(word_valid(e,"kông"));
    e->candidates_from_ai=TRUE;learn_candidate(e,"kông");
    g_assert_true(word_valid(e,"KÔNG"));
    g_clear_pointer(&e->learned_words,g_hash_table_unref);
    g_assert_true(word_valid(e,"kông"));
    /* Modifier/lock keys must pass through without committing the syllable. */
    ibus_gotiengviet_engine_reset(e);
    e->spellcheck=FALSE;
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_a,0,0));
    const guint modifiers[]={IBUS_Shift_L,IBUS_Shift_R,IBUS_Caps_Lock,
        IBUS_Shift_Lock,IBUS_Num_Lock,IBUS_Scroll_Lock};
    for(guint i=0;i<G_N_ELEMENTS(modifiers);i++) {
        g_assert_false(ibus_gotiengviet_engine_process_key_event(engine,modifiers[i],0,0));
        g_assert_cmpstr(e->preedit->str,==,"a");
        g_assert_false(ibus_gotiengviet_engine_process_key_event(engine,modifiers[i],0,IBUS_RELEASE_MASK));
        g_assert_cmpstr(e->preedit->str,==,"a");
    }
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_S,0,IBUS_LOCK_MASK));
    g_assert_cmpstr(e->preedit->str,==,"á");
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_b,0,0));
    g_assert_cmpstr(e->preedit->str,==,"áb");
    /* Case follows each resolved key, not the previous word or lock mask. */
    const struct { const gchar *input, *want; guint mask; } case_sequences[]={
        {"Bawst", "Bắt", 0}, {"bawSt", "bắt", IBUS_SHIFT_MASK},
        {"DUOCJWD", "ĐƯỢC", IBUS_LOCK_MASK}, {"duocjwd", "được", 0},
        {"duocjwd", "được", IBUS_LOCK_MASK | IBUS_SHIFT_MASK}
    };
    for(guint i=0;i<G_N_ELEMENTS(case_sequences);i++) {
        ibus_gotiengviet_engine_reset(e);
        for(const gchar *p=case_sequences[i].input;*p;p++)
            g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,*p,0,case_sequences[i].mask));
        g_assert_cmpstr(e->preedit->str,==,case_sequences[i].want);
        g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_space,0,0));
        g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_a,0,0));
        g_assert_cmpstr(e->preedit->str,==,"a");
    }
    const struct {const gchar *text,*preedit,*want;guint cursor,anchor;} snapshots[]={
        {"Dòng trước\nXin chào.","","Xin chào.",20,20},
        {"Xin chào bạn","","chào",8,4},
        {"Xin ","chào","Xin chào",4,4},
        {NULL,"được","được",0,0},
        {"Xin chào","dở","chào",4,8},
        {"abc","","abc",999,999}
    };
    for(guint i=0;i<G_N_ELEMENTS(snapshots);i++){
        gchar *input=assistant_input(snapshots[i].text,snapshots[i].cursor,snapshots[i].anchor,snapshots[i].preedit);
        g_assert_cmpstr(input,==,snapshots[i].want);g_free(input);
    }
    gchar *capture=g_build_filename(directory,"assistant-input",NULL);

    g_setenv("GTV_ASSISTANT_CAPTURE",capture,TRUE);
    ibus_gotiengviet_engine_reset(e);g_string_assign(e->typed_text,"Tôi ");g_string_assign(e->preedit,"được");
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_t,0,IBUS_CONTROL_MASK));
    gchar *captured=NULL;
    for(guint i=0;i<100 && !captured;i++){
        while(g_main_context_iteration(NULL,FALSE));
        g_file_get_contents(capture,&captured,NULL,NULL);
        if(!captured)g_usleep(10000);
    }
    g_assert_cmpstr(captured,==,"Tôi được");g_free(captured);g_remove(capture);g_free(capture);
    g_assert_cmpstr(e->preedit->str,==,"");
    g_assert_true(e->assistant_key_down);

    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_t,0,IBUS_CONTROL_MASK));
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_t,0,IBUS_RELEASE_MASK));
    g_assert_false(e->assistant_key_down);
    e->purpose=IBUS_INPUT_PURPOSE_PASSWORD;
    g_assert_false(ibus_gotiengviet_engine_process_key_event(engine,IBUS_t,0,IBUS_CONTROL_MASK));
    for(guint i=0;i<100 && e->assistant_running;i++){
        while(g_main_context_iteration(NULL,FALSE));g_usleep(10000);
    }
    g_assert_false(e->assistant_running);
    e->purpose=IBUS_INPUT_PURPOSE_FREE_FORM;
    g_free(e->focus_id);e->focus_id=g_strdup("/input/original");
    g_free(e->target_id);e->target_id=g_strdup("/input/original");
    g_free(e->target_text);e->target_text=g_strdup("Xin chào.");
    e->target_backspaces=FALSE;
    e->target_cursor=e->target_anchor=9;e->target_length=9;
    e->replacement=g_strdup("Hello.");e->replacement_wait=0;
    IBusText *snapshot=ibus_text_new_from_string("Đã sửa");g_object_ref_sink(snapshot);
    g_signal_emit_by_name(engine,"set-surrounding-text",snapshot,6,6);g_object_unref(snapshot);
    g_assert_true(apply_replacement(e));g_assert_nonnull(e->replacement);
    snapshot=ibus_text_new_from_string("Xin chào.");g_object_ref_sink(snapshot);
    g_signal_emit_by_name(engine,"set-surrounding-text",snapshot,9,9);g_object_unref(snapshot);
    g_free(e->focus_id);e->focus_id=g_strdup("/input/other");
    g_assert_true(apply_replacement(e));g_assert_nonnull(e->replacement);
    g_free(e->focus_id);e->focus_id=g_strdup("/input/original");
    /* Exercise Enter through the adapter and apply the actual D-Bus signals in a client. */
    g_dbus_connection_flush_sync(connection,NULL,NULL);
    while(g_main_context_iteration(NULL,FALSE));
    application_text=g_string_new("Xin chào.");application_cursor=9;
    guint subscription=g_dbus_connection_signal_subscribe(connection,NULL,IBUS_INTERFACE_ENGINE,NULL,
        "/org/freedesktop/IBus/Engine/Test",NULL,G_DBUS_SIGNAL_FLAGS_NONE,application_signal,NULL,NULL);
    e->assistant_inline=TRUE;
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_Return,0,0));
    /* Enter must return before any callback into the application. */
    g_assert_nonnull(e->replacement);
    g_assert_true(e->replacing);
    g_assert_cmpuint(replacement_signals,==,0);
    for(guint i=0;i<100 && replacement_signals<2;i++){
        while(g_main_context_iteration(NULL,FALSE));g_usleep(10000);
    }
    g_assert_cmpuint(replacement_signals,==,2);
    g_assert_cmpstr(application_text->str,==,"Hello.");
    g_string_free(application_text,TRUE);g_dbus_connection_signal_unsubscribe(connection,subscription);
    g_assert_null(e->replacement);
    g_assert_true(g_str_has_suffix(e->typed_text->str,"Hello."));
    /* The reported 0x9 client does not implement surrounding-text deletion. */
    e->caps=IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS;
    g_string_assign(e->typed_text,"Xin chào.");
    g_assert_false(ibus_gotiengviet_engine_process_key_event(engine,IBUS_Super_L,0,0));
    g_assert_cmpstr(e->typed_text->str,==,"Xin chào.");
    e->target_backspaces=TRUE;e->target_length=9;
    e->replacement=g_strdup("Hello.");e->assistant_cancelled=FALSE;
    /* No accessible selection means no destructive guess and no blind insertion. */
    g_assert_true(apply_replacement(e));g_assert_nonnull(e->replacement);
    g_assert_cmpstr(e->typed_text->str,==,"Xin chào.");g_clear_pointer(&e->replacement,g_free);
    g_assert_true(backspace_text_supported("Tôi được "));
    g_assert_false(backspace_text_supported("😊"));g_assert_false(backspace_text_supported("a\xcc\x81"));
    e->cursor_known=TRUE;e->cursor_expected=FALSE;e->last_cursor=(IBusRectangle){0,0,1,1};
    e->replacement=g_strdup("stale");
    /* A multiline selection moves the caret as part of the replacement itself. */
    e->replacing=TRUE;
    ibus_gotiengviet_engine_set_cursor_location(engine,25,0,1,1);
    g_assert_false(e->assistant_cancelled);g_assert_nonnull(e->replacement);
    g_assert_cmpstr(e->typed_text->str,==,"Xin chào.");
    e->replacing=FALSE;
    ibus_gotiengviet_engine_set_cursor_location(engine,50,0,1,1);
    g_assert_true(e->assistant_cancelled);g_assert_null(e->replacement);g_assert_cmpstr(e->typed_text->str,==,"");
    /* Typing while inference runs invalidates that result without swallowing input. */
    ibus_gotiengviet_engine_reset(e);e->assistant_inline=TRUE;e->assistant_running=TRUE;
    e->assistant_cancelled=FALSE;
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_Return,0,IBUS_MOD2_MASK));
    g_assert_false(e->assistant_cancelled);g_assert_true(e->assistant_running);
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_a,0,0));
    g_assert_true(e->assistant_cancelled);g_assert_cmpstr(e->preedit->str,==,"a");
    e->assistant_running=FALSE;
    e->replacement=g_strdup("stale");e->replacement_wait=0;
    g_assert_true(apply_replacement(e));g_assert_nonnull(e->replacement);
    g_clear_pointer(&e->replacement,g_free);
    gint start,end;
    g_assert_true(gtv_text_range("Xin chào\xc2\xa0",9,9,"Xin chào ",&start,&end));
    g_assert_cmpint(start,==,0);g_assert_cmpint(end,==,9);
    g_assert_false(gtv_text_range("Xin chào\n",9,9,"Xin chào ",&start,&end));
    g_assert_true(gtv_text_range("Đầu câu: Xin chào.",18,18,"Xin chào.",&start,&end));
    g_assert_cmpint(start,==,9);g_assert_cmpint(end,==,18);
    g_assert_false(gtv_text_range("Khác",4,4,"Xin chào.",&start,&end));
    g_assert_true(gtv_text_range("Xin chào.",0,9,"Xin chào.",&start,&end));
    g_object_unref(engine);
    g_dbus_connection_close_sync(connection,NULL,NULL);g_object_unref(connection);
    g_test_dbus_down(test_bus);g_object_unref(test_bus);
    const gchar *names[]={"config","ai.conf"};
    for(guint i=0;i<G_N_ELEMENTS(names);i++) {gchar *path=g_build_filename(config_dir,names[i],NULL);g_remove(path);g_free(path);}
    gchar *learned=g_build_filename(config_dir,"learned-words.txt",NULL);g_remove(learned);g_free(learned);
    g_rmdir(config_dir);g_rmdir(directory);g_free(config_dir);g_free(directory);
    return 0;
}
