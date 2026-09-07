#define main assistant_application_main
#include "../cmd/assistant/main.c"
#undef main
#include <glib/gstdio.h>
static guint ticks,phase;
static gboolean verify(gpointer data){
    g_assert_cmpuint(++ticks,<,100);
    if(request)return G_SOURCE_CONTINUE;
    gchar *text=text_of(result);
    if(phase<2)g_assert_cmpstr(text,==,"không");
    else{
        g_assert_cmpstr(text,==,"");
        g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(status)),"invalid model name"));
        g_assert_true(gtk_widget_get_sensitive(run));
        set_text(result,"không");
    }
    g_free(text);
    if(phase==0){
        phase=1;gtk_combo_box_set_active(GTK_COMBO_BOX(action),0);
        generate(NULL,NULL);g_assert_nonnull(request);return G_SOURCE_CONTINUE;
    }
    if(phase==1){
        phase=2;gchar *dir=g_build_filename(g_get_user_config_dir(),"gotiengviet",NULL);
        GtvConfig config;gtv_config_load(&config,dir);g_free(config.model);config.model=g_strdup("invalid model");
        g_assert_true(gtv_config_save(&config,dir,NULL));gtv_config_clear(&config);g_free(dir);
        generate(NULL,NULL);g_assert_nonnull(request);return G_SOURCE_CONTINUE;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(action),1);
    gtk_entry_set_text(GTK_ENTRY(language),"日本語");preferences(TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(action),0);gtk_entry_set_text(GTK_ENTRY(language),"");
    preferences(FALSE);
    g_assert_cmpint(gtk_combo_box_get_active(GTK_COMBO_BOX(action)),==,1);
    g_assert_cmpstr(gtk_entry_get_text(GTK_ENTRY(language)),==,"日本語");
    GdkEventKey enter={.keyval=GDK_KEY_Return};
    g_assert_true(accept_result(gtk_widget_get_toplevel(source),&enter,NULL));
    return G_SOURCE_REMOVE;
}
static gboolean start(gpointer data){
    generate(NULL,NULL);g_assert_null(request); /* Empty input must not send. */
    set_text(source,"a\"b");gtk_combo_box_set_active(GTK_COMBO_BOX(action),1);
    gtk_entry_set_text(GTK_ENTRY(language),"");
    generate(NULL,NULL);g_assert_null(request); /* Translation needs a language. */
    gtk_entry_set_text(GTK_ENTRY(language),"English");auto_generate(NULL);
    g_assert_nonnull(request);g_assert_false(gtk_widget_get_sensitive(run));
    g_timeout_add(50,verify,NULL);return G_SOURCE_REMOVE;
}
int main(int argc,char **argv){
    gchar *dir=g_dir_make_tmp("gtv-assistant-test-XXXXXX",NULL);
    g_setenv("XDG_CONFIG_HOME",dir,TRUE);
    gchar *cfg=g_build_filename(dir,"gotiengviet",NULL);GtvConfig config;
    gtv_config_load(&config,cfg);g_free(config.model);config.model=g_strdup("qwen2:0.5b");
    g_assert_true(gtv_config_save(&config,cfg,NULL));gtv_config_clear(&config);
    gchar *path=g_strconcat(g_getenv("GTV_TEST_CURL_DIR"),G_SEARCHPATH_SEPARATOR_S,g_getenv("PATH"),NULL);
    g_setenv("PATH",path,TRUE);g_free(path);
    g_setenv("GDK_BACKEND","broadway",TRUE);g_setenv("BROADWAY_DISPLAY",":96",TRUE);
    GSubprocess *display=g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_SILENCE,NULL,"broadwayd","--address=127.0.0.1","--port=18096",":96",NULL);
    g_assert_nonnull(display);gboolean ready=FALSE;
    for(int i=0;i<30 && !ready;i++){g_usleep(100000);ready=gtk_init_check(&argc,&argv);}
    g_assert_true(ready);g_timeout_add(100,start,NULL);
    int rc=assistant_application_main(argc,argv);
    g_subprocess_force_exit(display);g_subprocess_wait(display,NULL,NULL);g_object_unref(display);
    const gchar *names[]={"config","ai.conf","assistant.conf"};
    for(guint i=0;i<G_N_ELEMENTS(names);i++){gchar *p=g_build_filename(cfg,names[i],NULL);g_remove(p);g_free(p);}
    g_rmdir(cfg);g_rmdir(dir);g_free(cfg);g_free(dir);return rc;
}
