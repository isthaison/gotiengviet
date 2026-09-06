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
        {"ass","as",TRUE},{"duoc579","được",FALSE},{"[[","[",TRUE},{":smile:",":smile:",TRUE}
    };
    for(guint i=0;i<G_N_ELEMENTS(cases);i++) {
        ibus_gotiengviet_engine_reset(e);e->mode_telex=cases[i].telex;
        for(const gchar *p=cases[i].input;*p;p++)
            g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,(guint)*p,0,0));
        g_assert_cmpstr(e->preedit->str,==,cases[i].want);
    }
    g_assert_true(ibus_gotiengviet_engine_process_key_event(engine,IBUS_space,0,0));
    g_assert_cmpstr(e->preedit->str,==,"");
    g_object_unref(engine);
    g_dbus_connection_close_sync(connection,NULL,NULL);g_object_unref(connection);
    g_test_dbus_down(test_bus);g_object_unref(test_bus);
    const gchar *names[]={"config","ai.conf"};
    for(guint i=0;i<G_N_ELEMENTS(names);i++) {gchar *path=g_build_filename(config_dir,names[i],NULL);g_remove(path);g_free(path);}
    g_rmdir(config_dir);g_rmdir(directory);g_free(config_dir);g_free(directory);
    return 0;
}
