#include <gtk/gtk.h>
#include <stdio.h>
static GtkWidget *entry;
static gboolean insert(GIOChannel *channel,GIOCondition condition,gpointer data){
 gchar *line=NULL;gsize length=0;g_io_channel_read_line(channel,&line,&length,NULL,NULL);
 if(line){g_strchomp(line);gint pos=gtk_editable_get_position(GTK_EDITABLE(entry));gtk_editable_delete_selection(GTK_EDITABLE(entry));pos=gtk_editable_get_position(GTK_EDITABLE(entry));gtk_editable_insert_text(GTK_EDITABLE(entry),line,-1,&pos);gtk_editable_set_position(GTK_EDITABLE(entry),pos);g_free(line);}
 return TRUE;
}
int main(int argc,char **argv){gtk_init(&argc,&argv);GtkWidget *w=gtk_window_new(GTK_WINDOW_TOPLEVEL);gtk_window_set_title(GTK_WINDOW(w),"GoTiengViet — kiểm thử thay câu");entry=gtk_entry_new();gtk_entry_set_text(GTK_ENTRY(entry),"GTV probe 7294 — Xin chào.");gtk_container_add(GTK_CONTAINER(w),entry);gtk_widget_show_all(w);gtk_window_present(GTK_WINDOW(w));gtk_widget_grab_focus(entry);gtk_editable_set_position(GTK_EDITABLE(entry),-1);GIOChannel *c=g_io_channel_unix_new(0);g_io_add_watch(c,G_IO_IN,insert,NULL);gtk_main();}
