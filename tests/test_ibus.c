#define main ibus_application_main
#include "../ibus/engine.c"
#undef main
#include <glib/gstdio.h>

int main(int argc,char **argv) {
    g_test_init(&argc,&argv,NULL);
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
    IBusEngine *engine=ibus_engine_new_with_type(ibus_gotiengviet_engine_get_type(),"gotiengviet",
        "/org/freedesktop/IBus/Engine/Test",connection);
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
    e->spellcheck=TRUE;e->mode_telex=TRUE;
    g_string_assign(e->sentence_context,"xin");
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_c,0,0));
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_h,0,0));
    g_assert_cmpint(e->n_candidates,>,0);
    g_assert_cmpstr(e->candidates[0],==,"chào");
    g_assert_cmpint(e->n_candidates,<=,5);
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
    g_object_unref(engine);
    g_dbus_connection_close_sync(connection,NULL,NULL);g_object_unref(connection);
    g_test_dbus_down(test_bus);g_object_unref(test_bus);
    const gchar *names[]={"config","ai.conf"};
    for(guint i=0;i<G_N_ELEMENTS(names);i++) {gchar *path=g_build_filename(config_dir,names[i],NULL);g_remove(path);g_free(path);}
    g_rmdir(config_dir);g_rmdir(directory);g_free(config_dir);g_free(directory);
    return 0;
}
