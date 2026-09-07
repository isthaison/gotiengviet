#include <gtk/gtk.h>
#include "internal.h"

static GtkWidget *source, *result, *action, *language, *run, *status;
static GSubprocess *request;
static GCancellable *cancel;
static gboolean return_result, headless;
static int application_status;
static void preferences(gboolean save){
    gchar *directory=g_build_filename(g_get_user_config_dir(),"gotiengviet",NULL);
    gchar *path=g_build_filename(directory,"assistant.conf",NULL);
    GKeyFile *file=g_key_file_new();
    if(save){
        g_key_file_set_integer(file,"assistant","action",gtk_combo_box_get_active(GTK_COMBO_BOX(action)));
        g_key_file_set_string(file,"assistant","language",gtk_entry_get_text(GTK_ENTRY(language)));
        if(g_mkdir_with_parents(directory,0700)==0)g_key_file_save_to_file(file,path,NULL);
    }else{
        gchar *defpath=gtv_data_path("assistant.conf");
        GKeyFile *defs=g_key_file_new();
        gboolean dloaded=g_key_file_load_from_file(defs,defpath,G_KEY_FILE_NONE,NULL);
        gint selected=dloaded ? g_key_file_get_integer(defs,"assistant","action",NULL) : 0;
        gchar *target=dloaded ? g_key_file_get_string(defs,"assistant","language",NULL) : NULL;
        g_key_file_unref(defs);g_free(defpath);
        if(g_key_file_load_from_file(file,path,G_KEY_FILE_NONE,NULL)){
            selected=g_key_file_get_integer(file,"assistant","action",NULL);
            g_free(target);target=g_key_file_get_string(file,"assistant","language",NULL);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(action),selected==1?1:0);
        gtk_entry_set_text(GTK_ENTRY(language),target && *target ? target : "English");
        g_free(target);
    }
    g_key_file_unref(file);g_free(path);g_free(directory);
}
static gchar *text_of(GtkWidget *view){
    GtkTextBuffer *b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    GtkTextIter start,end;gtk_text_buffer_get_bounds(b,&start,&end);
    return gtk_text_buffer_get_text(b,&start,&end,FALSE);
}
static void set_text(GtkWidget *view,const gchar *text){
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),text,-1);
}
static void finished(GObject *object,GAsyncResult *res,gpointer data){
    gchar *out=NULL,*diagnostic=NULL;GError *error=NULL;
    gboolean ok=g_subprocess_communicate_utf8_finish(G_SUBPROCESS(object),res,&out,&diagnostic,&error);
    gchar *answer=ok && g_subprocess_get_successful(G_SUBPROCESS(object)) ? gtv_json_response(out) : NULL;
    if(answer && *g_strstrip(answer)){
        set_text(result,answer);gtk_label_set_text(GTK_LABEL(status),return_result ? "Enter để thay câu gốc, Esc để hủy." : "Enter để sao chép kết quả (ô nhập không hỗ trợ thay trực tiếp).");
    }else{
        const gchar *detail=out && *out ? out : diagnostic && *diagnostic ? diagnostic : error ? error->message : "Phản hồi trống hoặc không hợp lệ.";
        gchar *short_detail=g_utf8_substring(detail,0,MIN(g_utf8_strlen(detail,-1),400));
        gchar *message=g_strconcat("Ollama: ",short_detail,NULL);
        gtk_label_set_text(GTK_LABEL(status),message);g_free(message);g_free(short_detail);
    }
    if(headless){
        if(answer && *answer){fputs(answer,stdout);fflush(stdout);}
        else{g_printerr("%s\n",gtk_label_get_text(GTK_LABEL(status)));application_status=1;}
        gtk_main_quit();
    }
    g_free(answer);g_free(out);g_free(diagnostic);g_clear_error(&error);g_clear_object(&request);
    gtk_widget_set_sensitive(run,TRUE);
}
static void generate(GtkButton *button,gpointer data){
    if(request)return;
    gchar *input=text_of(source);
    if(!*g_strstrip(input)){g_free(input);gtk_label_set_text(GTK_LABEL(status),"Nhập đoạn văn cần xử lý.");return;}
    if(strlen(input)>32768){g_free(input);gtk_label_set_text(GTK_LABEL(status),"Đoạn văn quá dài (tối đa 32 KB).");return;}
    const gchar *target=gtk_entry_get_text(GTK_ENTRY(language));
    gboolean translate=gtk_combo_box_get_active(GTK_COMBO_BOX(action))==1;
    if(translate && !*target){g_free(input);gtk_label_set_text(GTK_LABEL(status),"Nhập ngôn ngữ đích.");return;}
    preferences(TRUE);
    set_text(result,"");
    GtvConfig config;gchar *dir=g_build_filename(g_get_user_config_dir(),"gotiengviet",NULL);
    gtv_config_load(&config,dir);g_free(dir);
    const gchar *pgroup=translate ? "assistant_translate" : "assistant_rewrite";
    gchar *itempl=gtv_prompt_get(pgroup,"instruction",NULL);
    gchar *sysraw=gtv_prompt_get(pgroup,"system",NULL);
    if(!itempl || !sysraw){
        g_free(itempl);g_free(sysraw);g_free(input);gtv_config_clear(&config);g_free(dir);
        gtk_label_set_text(GTK_LABEL(status),"Thiếu mẫu prompt (prompts.conf). Hãy cài lại GoTiengViet.");
        return;
    }
    gchar *instruction;
    if(translate){const gchar *a[]={target,input};instruction=gtv_format_template(itempl,a,2);}
    else{const gchar *a[]={input};instruction=gtv_format_template(itempl,a,1);}
    g_free(itempl);
    gchar *system=gtv_json_quote(sysraw);g_free(sysraw);
    gchar *prompt=gtv_json_quote(instruction),*model=gtv_json_quote(config.model);
    gchar *body=g_strdup_printf("{\"model\":%s,\"system\":%s,\"prompt\":%s,\"stream\":false,\"options\":{\"temperature\":0}}",model,system,prompt);
    gchar *url=g_strconcat(config.url,"/api/generate",NULL);
    GError *error=NULL;
    request=g_subprocess_new(G_SUBPROCESS_FLAGS_STDIN_PIPE|G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_PIPE,&error,
        "curl","--silent","--show-error","--fail-with-body","--max-time","120","--max-filesize","1048576","--proto","=http,https",
        "--url",url,"--header","Content-Type: application/json","--data-binary","@-",NULL);
    if(request){
        gtk_widget_set_sensitive(run,FALSE);gtk_label_set_text(GTK_LABEL(status),"Ollama đang xử lý…");
        g_subprocess_communicate_utf8_async(request,body,cancel,finished,NULL);
    }else gtk_label_set_text(GTK_LABEL(status),"Không chạy được curl. Hãy cài curl rồi thử lại.");
    g_clear_error(&error);gtv_config_clear(&config);g_free(input);g_free(instruction);g_free(system);g_free(prompt);g_free(model);g_free(body);g_free(url);
}
static gboolean auto_generate(gpointer data){
    gchar *input=text_of(source);
    if(*g_strstrip(input))generate(NULL,NULL);
    g_free(input);return G_SOURCE_REMOVE;
}
static void action_changed(GtkComboBox *combo,gpointer data){
    gtk_widget_set_visible(language,gtk_combo_box_get_active(combo)==1);
}
static void copy_result(GtkButton *button,gpointer data){
    gchar *text=text_of(result);
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),text,-1);
    gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));g_free(text);
}
static gboolean accept_result(GtkWidget *window,GdkEventKey *event,gpointer data){
    if(event->keyval==GDK_KEY_Escape){gtk_widget_destroy(window);return TRUE;}
    if(event->keyval!=GDK_KEY_Return && event->keyval!=GDK_KEY_KP_Enter)return FALSE;
    if(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))return FALSE;
    if(request)return TRUE;
    gchar *text=text_of(result);
    if(!*g_strstrip(text)){g_free(text);generate(NULL,NULL);return TRUE;}
    copy_result(NULL,NULL);
    if(return_result){fputs(text,stdout);fflush(stdout);}
    g_free(text);gtk_widget_destroy(window);return TRUE;
}
static void use_result(GtkButton *button,gpointer window){
    GdkEventKey enter={.keyval=GDK_KEY_Return};accept_result(GTK_WIDGET(window),&enter,NULL);
}
static void close_window(GtkWidget *widget,gpointer data){
    g_cancellable_cancel(cancel);if(request)g_subprocess_force_exit(request);gtk_main_quit();
}
static GtkWidget *add_editor(GtkWidget *box,const gchar *label){
    gtk_box_pack_start(GTK_BOX(box),gtk_label_new(label),FALSE,FALSE,0);
    GtkWidget *scroll=gtk_scrolled_window_new(NULL,NULL),*view=gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view),GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll),view);gtk_box_pack_start(GTK_BOX(box),scroll,TRUE,TRUE,0);return view;
}
int main(int argc,char **argv){
    gboolean from_stdin=FALSE;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--stdin"))from_stdin=TRUE;
        if(!strcmp(argv[i],"--replace"))return_result=TRUE;
        if(!strcmp(argv[i],"--headless"))headless=TRUE;
    }
    gtk_init(&argc,&argv);cancel=g_cancellable_new();
    GtkWidget *window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window),"GoTiengViet — Viết lại / Dịch với Ollama");
    gtk_window_set_default_size(GTK_WINDOW(window),540,380);
    g_signal_connect(window,"key-press-event",G_CALLBACK(accept_result),NULL);
    g_signal_connect(window,"destroy",G_CALLBACK(close_window),NULL);
    GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);gtk_container_set_border_width(GTK_CONTAINER(box),12);gtk_container_add(GTK_CONTAINER(window),box);
    GtkWidget *details=gtk_expander_new("Nội dung gốc"),*source_box=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    gtk_container_add(GTK_CONTAINER(details),source_box);
    gtk_box_pack_start(GTK_BOX(box),details,FALSE,FALSE,0);
    source=add_editor(source_box,"Nội dung đã lấy tự động");
    gtk_widget_set_size_request(source, -1,90);
    gtk_expander_set_expanded(GTK_EXPANDER(details),!from_stdin);
    if(from_stdin){GString *s=g_string_new("");int c;while(s->len<32768 && (c=getchar())!=EOF)g_string_append_c(s,c);
        if(g_utf8_validate(s->str,-1,NULL))set_text(source,s->str);
        g_string_free(s,TRUE);}
    action=gtk_combo_box_text_new();gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action),"Viết lại cho rõ ràng, tự nhiên");gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(action),"Dịch sang ngôn ngữ khác");gtk_combo_box_set_active(GTK_COMBO_BOX(action),0);gtk_box_pack_start(GTK_BOX(box),action,FALSE,FALSE,0);
    language=gtk_entry_new();gtk_entry_set_placeholder_text(GTK_ENTRY(language),"Ngôn ngữ đích, ví dụ: English, 日本語, Tiếng Việt");gtk_box_pack_start(GTK_BOX(box),language,FALSE,FALSE,0);
    run=gtk_button_new_with_label("Xử lý lại");g_signal_connect(run,"clicked",G_CALLBACK(generate),NULL);gtk_box_pack_start(GTK_BOX(box),run,FALSE,FALSE,0);
    result=add_editor(box,"Kết quả");
    GtkWidget *copy=gtk_button_new_with_label(return_result ? "Thay câu gốc ↵" : "Sao chép kết quả ↵");
    g_signal_connect(copy,"clicked",G_CALLBACK(use_result),window);gtk_box_pack_start(GTK_BOX(box),copy,FALSE,FALSE,0);
    status=gtk_label_new("Tự lấy nội dung vừa gõ. Enter để dùng kết quả; Shift+Enter để xuống dòng.");gtk_label_set_line_wrap(GTK_LABEL(status),TRUE);gtk_box_pack_start(GTK_BOX(box),status,FALSE,FALSE,0);
    preferences(FALSE);
    g_signal_connect(action,"changed",G_CALLBACK(action_changed),NULL);
    if(!headless)gtk_widget_show_all(window);
    action_changed(GTK_COMBO_BOX(action),NULL);
    if(from_stdin)g_idle_add(auto_generate,NULL);
    gtk_main();return application_status;
}
