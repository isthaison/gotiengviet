/* Opt-in desktop integration probe. Only edits its own disposable GTK entry. */
#include <gtk/gtk.h>
#include <ibus.h>
#include <string.h>
static IBusInputContext *context;
static GtkWidget *entry;
static gchar *expected;
static GtkWidget *probe_window;
static gboolean passed;
static gboolean uppercase;
static gboolean finish(gpointer unused){
    passed=expected && !strcmp(gtk_entry_get_text(GTK_ENTRY(entry)),expected);
    g_print("Ctrl+T / Ollama / Enter / GTK text equality: %s\n",passed?"PASS":"FAIL");
    gtk_main_quit();return G_SOURCE_REMOVE;
}
static void commit(IBusInputContext *ctx,IBusText *text,gpointer unused){
    gtk_editable_delete_selection(GTK_EDITABLE(entry));
    gint position=gtk_editable_get_position(GTK_EDITABLE(entry));
    gtk_editable_insert_text(GTK_EDITABLE(entry),text->text,-1,&position);
    gtk_editable_set_position(GTK_EDITABLE(entry),position);
    if(expected)g_timeout_add(600,finish,NULL);
}
static gboolean accept(gpointer unused){
    ibus_input_context_process_key_event(context,IBUS_Return,0,0);return G_SOURCE_REMOVE;
}
static void auxiliary(IBusInputContext *ctx,IBusText *text,gboolean visible,gpointer unused){
    g_print("Auxiliary visible=%d bytes=%zu\n",visible,strlen(text->text));
    const gchar *footer=strstr(text->text,"\nEnter: thay câu");
    if(visible && footer && !expected){
        const gchar *source=uppercase ? "XIN CHÀO " : "xin chào ";
        if(strcmp(gtk_entry_get_text(GTK_ENTRY(entry)),source)){
            g_printerr("Source did not match the complete typed sample.\n");gtk_main_quit();return;
        }
        expected=g_strndup(text->text,footer-text->text);g_idle_add(accept,NULL);}
}
static gboolean type_sample(gpointer unused){
    IBusEngineDesc *desc=ibus_input_context_get_engine(context);
    g_print("Probe engine: %s\n",desc?ibus_engine_desc_get_name(desc):"none");
    if(desc)g_object_unref(desc);
    const gchar *keys=uppercase ? "XIN CHAOF " : "xin chaof ";
    for(const gchar *p=keys;*p;p++)ibus_input_context_process_key_event(context,*p,0,0);
    gboolean handled=ibus_input_context_process_key_event(context,IBUS_t,0,IBUS_CONTROL_MASK);
    g_print("Ctrl+T handled: %d\n",handled);
    ibus_input_context_process_key_event(context,IBUS_t,0,IBUS_CONTROL_MASK|IBUS_RELEASE_MASK);
    return G_SOURCE_REMOVE;
}
static gboolean begin(gpointer unused){
    if(!gtk_window_is_active(GTK_WINDOW(probe_window))){g_printerr("Fixture did not get desktop focus; no input sent.\n");gtk_main_quit();return G_SOURCE_REMOVE;}
    ibus_input_context_focus_in(context);ibus_input_context_set_engine(context,"gotiengviet");
    g_timeout_add(500,type_sample,NULL);
    return G_SOURCE_REMOVE;
}
static gboolean timeout(gpointer unused){g_printerr("Timed out waiting for replacement.\n");gtk_main_quit();return G_SOURCE_REMOVE;}
int main(int argc,char **argv){
    uppercase=argc>1 && !strcmp(argv[1],"--uppercase");
    g_setenv("GTK_IM_MODULE","gtk-im-context-simple",TRUE);gtk_init(&argc,&argv);ibus_init();
    IBusBus *bus=ibus_bus_new();if(!ibus_bus_is_connected(bus))return 2;
    context=ibus_bus_create_input_context(bus,"gtv-assistant-live-fixture");if(!context)return 3;
    g_object_ref_sink(context);
    g_signal_connect(context,"commit-text",G_CALLBACK(commit),NULL);
    g_signal_connect(context,"update-auxiliary-text",G_CALLBACK(auxiliary),NULL);
    ibus_input_context_set_capabilities(context,IBUS_CAP_PREEDIT_TEXT|IBUS_CAP_FOCUS|IBUS_CAP_AUXILIARY_TEXT);
    GtkWidget *window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window),"GoTiengViet — kiểm thử Ctrl+T (tự đóng)");
    probe_window=window;
    entry=gtk_entry_new();g_object_set(entry,"im-module","gtk-im-context-simple",NULL);gtk_widget_set_size_request(entry,480,60);gtk_container_add(GTK_CONTAINER(window),entry);
    gtk_widget_show_all(window);gtk_window_present(GTK_WINDOW(window));gtk_widget_grab_focus(entry);
    g_timeout_add(600,begin,NULL);g_timeout_add_seconds(20,timeout,NULL);gtk_main();
    ibus_input_context_focus_out(context);ibus_proxy_destroy(IBUS_PROXY(context));g_object_unref(context);g_object_unref(bus);
    gtk_widget_destroy(window);g_free(expected);return passed?0:1;
}
