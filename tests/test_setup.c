/* Exercise the actual native GTK UI in an isolated Broadway display. */
#define main setup_application_main
#include "../cmd/setup/main.c"
#undef main

static gboolean verify_setup(gpointer data) {
    (void)data;
    g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_vni)));
    g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_modern)));
    g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_spell)));
    g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_ai)));
    g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(entry_url)),==,"http://localhost:55603");
    g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_port)),==,55603);
    g_assert_cmpstr(gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_model)),==,"qwen2:1.5b");
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}
int main(int argc,char **argv) {
    g_setenv("GDK_BACKEND","broadway",TRUE);
    g_setenv("BROADWAY_DISPLAY",":95",TRUE);
    GError *error=NULL;
    GSubprocess *display=g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error,"broadwayd","--address=127.0.0.1","--port=18095",":95",NULL);
    g_assert_no_error(error);g_assert_nonnull(display);
    gboolean connected=FALSE;
    for(int i=0;i<30 && !connected;i++) {g_usleep(100000);connected=gtk_init_check(&argc,&argv);}
    if(!connected) {g_subprocess_force_exit(display);g_subprocess_wait(display,NULL,NULL);g_object_unref(display);g_error("Cannot open isolated Broadway display");}
    g_timeout_add(100,verify_setup,NULL);
    int result=setup_ui(argc,argv,"vni","false","false","false","qwen2:1.5b","http://localhost:55603","55603","/tmp/gotiengviet-ui-test-config");
    g_subprocess_force_exit(display);g_subprocess_wait(display,NULL,NULL);g_object_unref(display);
    return result;
}
