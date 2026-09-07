#include "../../engine/engine.h"
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Existing desktop integration commands; report launch failures. */
static int run_shell(const char *command) {
    int status = system(command);
    if (status != 0) g_warning("Desktop command failed (status %d)", status);
    return status;
}

// --- Tray indicator (một phần của ứng dụng duy nhất, chạy với cờ --tray) ---
static AppIndicator *tray_indicator;
static GtkWidget *tray_item_telex;
static GtkWidget *tray_item_vni;

static char* tray_current_method(){
    char *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    GKeyFile *kf = g_key_file_new();
    char *method = NULL;
    if(g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)){
        method = g_key_file_get_string(kf, "input", "method", NULL);
    }
    g_key_file_free(kf);
    g_free(path);
    if(!method) method = g_strdup("telex");
    return method;
}

static void tray_update_config_method(const char *method){
    char *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    char *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    GKeyFile *kf = g_key_file_new();
    g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL);
    // Giữ nguyên các key khác ([ai]...), chỉ đổi input/method
    g_key_file_set_string(kf, "input", "method", method);
    if(!g_key_file_has_key(kf, "input", "modern", NULL))
        g_key_file_set_string(kf, "input", "modern", "true");
    if(!g_key_file_has_key(kf, "input", "spellcheck", NULL))
        g_key_file_set_string(kf, "input", "spellcheck", "true");
    gsize len = 0;
    gchar *data = g_key_file_to_data(kf, &len, NULL);
    if(data) g_file_set_contents(path, data, (gssize)len, NULL);
    g_free(data);
    g_key_file_free(kf);
    g_free(path);
}

static void tray_update_indicator_label(gboolean is_telex){
    if(!tray_indicator) return;
    // Hiển thị kiểu gõ ngay trên statusbar bên cạnh icon
    app_indicator_set_label(tray_indicator, is_telex ? "Telex" : "VNI", is_telex ? "Telex" : "VNI");
    app_indicator_set_title(tray_indicator, is_telex ? "GoTiengViet — Telex" : "GoTiengViet — VNI");
}

static void tray_refresh_checks(){
    char *m = tray_current_method();
    gboolean is_telex = (g_strcmp0(m, "vni") != 0 && g_strcmp0(m, "VNI") != 0);
    // block signals while updating to avoid recursion
    g_signal_handlers_block_by_func(tray_item_telex, NULL, NULL);
    g_signal_handlers_block_by_func(tray_item_vni, NULL, NULL);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(is_telex ? tray_item_telex : tray_item_vni), TRUE);
    g_signal_handlers_unblock_by_func(tray_item_telex, NULL, NULL);
    g_signal_handlers_unblock_by_func(tray_item_vni, NULL, NULL);
    tray_update_indicator_label(is_telex);
    g_free(m);
}

static gboolean tray_check_config_timer(gpointer data){
    (void)data;
    char *path = g_build_filename(g_get_user_config_dir(), "gotiengviet", "config", NULL);
    struct stat st;
    static time_t tray_last_mtime = 0;
    if(stat(path, &st) == 0){
        if(tray_last_mtime != 0 && st.st_mtime != tray_last_mtime){
            tray_refresh_checks();
        }
        tray_last_mtime = st.st_mtime;
    }
    g_free(path);
    return G_SOURCE_CONTINUE;
}

static void tray_on_activate_setup(GtkMenuItem *item, gpointer data){
    (void)item; (void)data;
    gchar *self = g_file_read_link("/proc/self/exe", NULL);
    if(self){
        gchar *setup_argv[] = {self, NULL};
        GError *err = NULL;
        if(g_spawn_async(NULL, setup_argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err)){
            g_free(self);
            return;
        }
        if(err){ g_warning("Failed to spawn setup: %s", err->message); g_error_free(err); }
        g_free(self);
    }
    run_shell("/usr/bin/gotiengviet 2>/dev/null || gotiengviet 2>/dev/null || /usr/libexec/ibus-setup-gotiengviet 2>/dev/null &");
}
static void tray_on_activate_telex(GtkMenuItem *item, gpointer data){
    (void)data;
    if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) return;
    tray_update_config_method("telex");
    // Một engine duy nhất: đảm bảo input-sources chỉ có gotiengviet rồi kích hoạt nó
    run_shell("gsettings set org.gnome.desktop.input-sources sources \"[('xkb','us'),('ibus','gotiengviet')]\" 2>/dev/null; ibus engine gotiengviet 2>/dev/null &");
    run_shell("notify-send 'GoTiengViet' 'Đã chuyển sang Telex (s f r x j)' 2>/dev/null &");
    tray_refresh_checks();
}
static void tray_on_activate_vni(GtkMenuItem *item, gpointer data){
    (void)data;
    if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) return;
    tray_update_config_method("vni");
    run_shell("gsettings set org.gnome.desktop.input-sources sources \"[('xkb','us'),('ibus','gotiengviet')]\" 2>/dev/null; ibus engine gotiengviet 2>/dev/null &");
    run_shell("notify-send 'GoTiengViet' 'Đã chuyển sang VNI (1-5, 6-9)' 2>/dev/null &");
    tray_refresh_checks();
}
static void tray_on_activate_quit(GtkMenuItem *item, gpointer data){
    (void)item; (void)data;
    gtk_main_quit();
}

int tray_run(int argc, char *argv[]){
    g_set_prgname("gotiengviet");
    g_set_application_name("GoTiengViet");
    gtk_init(&argc, &argv);

    // Đảm bảo theme tìm thấy icon "gotiengviet" kể cả khi hicolor cache chưa cập nhật
    GtkIconTheme *theme = gtk_icon_theme_get_default();
    if(theme){
        gtk_icon_theme_append_search_path(theme, "/usr/share/gotiengviet/icons");
        gtk_icon_theme_append_search_path(theme, "/usr/share/icons/hicolor");
    }

    // AppIndicator — hiện trên top bar GNOME qua extension ubuntu-appindicators
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    tray_indicator = app_indicator_new("gotiengviet", "gotiengviet",
                                  APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    G_GNUC_END_IGNORE_DEPRECATIONS
    app_indicator_set_status(tray_indicator, APP_INDICATOR_STATUS_ACTIVE);
    // Trỏ trực tiếp vào thư mục chứa icon để Shell resolve đúng icon
    app_indicator_set_icon_theme_path(tray_indicator, "/usr/share/gotiengviet/icons");
    app_indicator_set_icon_full(tray_indicator, "gotiengviet", "GoTiengViet");
    app_indicator_set_title(tray_indicator, "GoTiengViet — Telex/VNI");

    GtkWidget *menu = gtk_menu_new();
    GSList *group = NULL;
    tray_item_telex = gtk_radio_menu_item_new_with_label(group, "Telex (s f r x j, w)");
    group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(tray_item_telex));
    tray_item_vni = gtk_radio_menu_item_new_with_label(group, "VNI (1-5, 6-9)");

    GtkWidget *tray_item_setup = gtk_menu_item_new_with_label("Mở GoTiengViet Setup...");
    GtkWidget *tray_item_quit = gtk_menu_item_new_with_label("Thoát");
    g_signal_connect(tray_item_telex, "toggled", G_CALLBACK(tray_on_activate_telex), NULL);
    g_signal_connect(tray_item_vni, "toggled", G_CALLBACK(tray_on_activate_vni), NULL);
    g_signal_connect(tray_item_setup, "activate", G_CALLBACK(tray_on_activate_setup), NULL);
    g_signal_connect(tray_item_quit, "activate", G_CALLBACK(tray_on_activate_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), tray_item_telex);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), tray_item_vni);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), tray_item_setup);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), tray_item_quit);
    gtk_widget_show_all(menu);

    app_indicator_set_menu(tray_indicator, GTK_MENU(menu));
    tray_refresh_checks();

    g_timeout_add(1000, (GSourceFunc)tray_check_config_timer, NULL);

    gtk_main();
    return 0;
}
// --- Hết phần tray ---

static GtkWidget *rb_telex, *rb_vni, *cb_modern, *cb_spell;
static GtkWidget *lbl_preview;
static GtkWidget *cb_ai, *combo_model, *entry_url, *spin_port, *lbl_ai_status, *progress_ai, *btn_download;
static GtkWidget *log_scroll = NULL, *log_view = NULL;
static GtkTextBuffer *log_buf = NULL;
static guint log_timer_id = 0;
static GPid serve_pid = 0;
static GPid install_pid = 0;
static char active_log_path[256] = "/tmp/ollama_serve.log";
static int serve_wait_left = 0;
static char *config_path;
static gboolean download_in_progress = FALSE;

static void update_preview(){
    gboolean modern = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_modern));
    const char *txt = modern ?
        "Preview modern (hoà): <tt>hoaf→hoà, hoas→hoá, huow→hươu, thoas→thoá</tt>" :
        "Preview traditional (hòa): <tt>hoaf→hòa, hoas→hóa, huow→hươu, thoas→thóa</tt>";
    gtk_label_set_markup(GTK_LABEL(lbl_preview), txt);
}
static void on_modern_toggled(GtkWidget *w, gpointer data){
    update_preview();
}

static void ai_log(const char *line){
    if(!log_buf || !line) return;
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(log_buf, &end);
    gtk_text_buffer_insert(log_buf, &end, line, -1);
    gtk_text_buffer_insert(log_buf, &end, "\n", -1);
    int lines = gtk_text_buffer_get_line_count(log_buf);
    if(lines > 200){
        GtkTextIter start, cut;
        gtk_text_buffer_get_start_iter(log_buf, &start);
        gtk_text_buffer_get_iter_at_line(log_buf, &cut, lines - 200);
        gtk_text_buffer_delete(log_buf, &start, &cut);
    }
    if(log_scroll){
        GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(log_scroll));
        if(adj) gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj));
    }
}

static gboolean syncing_endpoint;
static gboolean local_endpoint(GUri *uri){
    const gchar *host=uri ? g_uri_get_host(uri) : NULL;
    return host && (!g_ascii_strcasecmp(host,"localhost") || !strcmp(host,"127.0.0.1") || !strcmp(host,"::1"));
}
static void port_changed(GtkSpinButton *spin,gpointer data){
    if(syncing_endpoint)return;
    GUri *uri=g_uri_parse(gtk_entry_get_text(GTK_ENTRY(entry_url)),G_URI_FLAGS_NONE,NULL);
    if(local_endpoint(uri)){
        gchar *url=g_uri_join(G_URI_FLAGS_NONE,g_uri_get_scheme(uri),g_uri_get_userinfo(uri),g_uri_get_host(uri),
            gtk_spin_button_get_value_as_int(spin),g_uri_get_path(uri),g_uri_get_query(uri),g_uri_get_fragment(uri));
        syncing_endpoint=TRUE;gtk_entry_set_text(GTK_ENTRY(entry_url),url);syncing_endpoint=FALSE;g_free(url);
    }
    if(uri)g_uri_unref(uri);
}
static void url_changed(GtkEditable *entry,gpointer data){
    if(syncing_endpoint)return;
    GUri *uri=g_uri_parse(gtk_entry_get_text(GTK_ENTRY(entry)),G_URI_FLAGS_NONE,NULL);
    if(local_endpoint(uri) && g_uri_get_port(uri)>=1024){
        syncing_endpoint=TRUE;gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_port),g_uri_get_port(uri));syncing_endpoint=FALSE;
    }
    if(uri)g_uri_unref(uri);
}
static gboolean ollama_serving(int port){
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "curl -s -m 2 http://localhost:%d/api/tags -o /dev/null 2>/dev/null", port);
    int rc = run_shell(cmd);
    return (rc == 0);
}

static gboolean poll_serve_log(gpointer data){
    static long last_off = 0;
    static char last_path[256] = "";
    (void)data;
    if(strcmp(last_path, active_log_path) != 0){
        snprintf(last_path, sizeof(last_path), "%s", active_log_path);
        last_off = 0;
    }
    FILE *f = fopen(active_log_path, "r");
    if(!f) return TRUE;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if(sz < last_off) last_off = 0;
    fseek(f, last_off, SEEK_SET);
    char line[1024];
    while(fgets(line, sizeof(line), f)){
        line[strcspn(line, "\r\n")] = 0;
        if(line[0]) ai_log(line);
    }
    last_off = ftell(f);
    fclose(f);
    return TRUE;
}

static void on_serve_exit(GPid pid, gint status, gpointer data){
    (void)data;
    g_spawn_close_pid(pid);
    if(pid == serve_pid) serve_pid = 0;
    char msg[160];
    snprintf(msg, sizeof(msg), "ollama serve đã thoát (mã %d). Xem chi tiết: /tmp/ollama_serve.log", status);
    if(lbl_ai_status) gtk_label_set_text(GTK_LABEL(lbl_ai_status), msg);
    ai_log(msg);
}

static gboolean check_serve_ready(gpointer data){
    (void)data;
    int port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_port));
    if(ollama_serving(port)){
        char msg[256];
        snprintf(msg, sizeof(msg), "Ollama đã chạy ở port %d (pid %d) — sẵn sàng gợi ý.", port, (int)serve_pid);
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), msg);
        ai_log(msg);
        if(log_timer_id == 0) log_timer_id = g_timeout_add(800, poll_serve_log, NULL);
        return FALSE;
    }
    if(--serve_wait_left <= 0){
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Sau 15s vẫn chưa kết nối được — xem log bên dưới (file /tmp/ollama_serve.log).");
        ai_log("CẢNH BÁO: quá 15s chưa thấy /api/tags. Kiểm tra log, port có bị chiếm không.");
        if(log_timer_id == 0) log_timer_id = g_timeout_add(800, poll_serve_log, NULL);
        return FALSE;
    }
    return TRUE;
}

static void ensure_ollama_serve(){
    int port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_port));
    if(ollama_serving(port)){
        char msg[256];
        snprintf(msg, sizeof(msg), "Ollama đang chạy ở port %d — sẵn sàng gợi ý.", port);
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), msg);
        ai_log(msg);
        if(log_timer_id == 0) log_timer_id = g_timeout_add(800, poll_serve_log, NULL);
        return;
    }
    if(!g_find_program_in_path("ollama")){
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Chưa cài Ollama — bấm nút 'Cài Ollama' bên dưới để cài trong app.");
        ai_log("Chưa có lệnh 'ollama'. Bấm nút 'Cài Ollama' để cài tự động (hỏi mật khẩu root 1 lần).");
        return;
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "Đang khởi động 'ollama serve' ở port %d...", port);
    gtk_label_set_text(GTK_LABEL(lbl_ai_status), msg);
    ai_log(msg);
    FILE *lf = fopen("/tmp/ollama_serve.log", "w");
    if(lf){ fprintf(lf, "=== ollama serve port %d (GoTiengViet Setup tự khởi động) ===\n", port); fclose(lf); }
    char shcmd[512];
    snprintf(shcmd, sizeof(shcmd), "OLLAMA_HOST=0.0.0.0:%d exec ollama serve >>/tmp/ollama_serve.log 2>&1", port);
    gchar *argv[] = {"sh", "-c", shcmd, NULL};
    GError *err = NULL;
    GPid pid = 0;
    if(!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, &err)){
        char em[256];
        snprintf(em, sizeof(em), "LỖI khởi động ollama: %s", err ? err->message : "unknown");
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), em);
        ai_log(em);
        if(err) g_error_free(err);
        return;
    }
    serve_pid = pid;
    g_child_watch_add(pid, on_serve_exit, NULL);
    ai_log("Đã chạy ollama serve, chờ /api/tags (tối đa 15s)...");
    serve_wait_left = 15;
    g_timeout_add(1000, check_serve_ready, NULL);
}

static void on_install_exit(GPid pid, gint status, gpointer data){
    (void)data;
    g_spawn_close_pid(pid);
    if(pid == install_pid) install_pid = 0;
    if(status == 0){
        ai_log("Cài Ollama xong. Tự khởi động serve...");
        snprintf(active_log_path, sizeof(active_log_path), "/tmp/ollama_serve.log");
        ensure_ollama_serve();
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "Cài Ollama thất bại (mã %d). Xem %s", status, active_log_path);
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), msg);
        ai_log(msg);
    }
}

static void on_install_clicked(GtkWidget *w, gpointer data){
    (void)w; (void)data;
    if(g_find_program_in_path("ollama")){
        ai_log("Ollama đã được cài — bỏ qua.");
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Ollama đã có. Check 'Bật AI' để tự chạy serve.");
        return;
    }
    if(install_pid != 0){
        ai_log("Đang cài Ollama, vui lòng đợi...");
        return;
    }
    FILE *lf = fopen("/tmp/ollama_install.log", "w");
    if(lf){ fprintf(lf, "=== Cài Ollama (GoTiengViet Setup) ===\n"); fclose(lf); }
    snprintf(active_log_path, sizeof(active_log_path), "/tmp/ollama_install.log");
    if(log_timer_id == 0) log_timer_id = g_timeout_add(800, poll_serve_log, NULL);
    gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Đang tải script cài Ollama...");
    ai_log("Tải https://ollama.com/install.sh ...");
    int rc = run_shell("curl -fsSL https://ollama.com/install.sh -o /tmp/ollama_install.sh >>/tmp/ollama_install.log 2>&1");
    if(rc != 0){
        ai_log("LỖI tải script cài đặt (mất mạng?). Thử lại sau.");
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Không tải được script cài Ollama (kiểm tra mạng).");
        return;
    }
    ai_log("Chạy cài đặt với quyền root (pkexec có thể hỏi mật khẩu)...");
    gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Đang cài Ollama — xem log bên dưới.");
    gchar *argv[] = {"pkexec", "sh", "-c", "sh /tmp/ollama_install.sh >>/tmp/ollama_install.log 2>&1", NULL};
    GError *err = NULL;
    GPid pid = 0;
    if(!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, &err)){
        char em[256];
        snprintf(em, sizeof(em), "LỖI gọi pkexec: %s", err ? err->message : "unknown");
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), em);
        ai_log(em);
        if(err) g_error_free(err);
        return;
    }
    install_pid = pid;
    g_child_watch_add(pid, on_install_exit, NULL);
}

static void on_ai_toggled(GtkWidget *w, gpointer data){
    (void)data;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))){
        ensure_ollama_serve();
    } else {
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), "AI tắt — dùng rule có sẵn, không cần Ollama.");
        ai_log("AI gợi ý: TẮT (ollama serve nền nếu đang chạy vẫn giữ nguyên).");
    }
}

static void on_save(GtkWidget *w, gpointer data){
    GtkWindow *win = GTK_WINDOW(data);
    const char *method = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_vni)) ? "vni" : "telex";
    const char *modern = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_modern)) ? "true" : "false";
    const char *spell = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_spell)) ? "true" : "false";
    const char *ai_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_ai)) ? "true" : "false";
    const char *model = g_strdup(gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_model)));
    if(!model) model = g_strdup("qwen2:0.5b");
    int port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_port));
    const char *url = gtk_entry_get_text(GTK_ENTRY(entry_url));
    // Nếu bật AI: đảm bảo ollama serve đang chạy rồi mới lưu
    if(strcmp(ai_enabled,"true")==0){ ensure_ollama_serve(); }
    GtvConfig config = {.mode = strcmp(method,"vni") == 0 ? GTV_VNI : GTV_TELEX,
        .modern = strcmp(modern,"true") == 0, .spellcheck = strcmp(spell,"true") == 0,
        .ai_enabled = strcmp(ai_enabled,"true") == 0, .model = (gchar *)model,
        .url = (gchar *)url, .port = g_strdup_printf("%d",port)};
    gchar *directory = g_path_get_dirname(config_path);
    GError *error = NULL;
    gboolean saved = gtv_config_save(&config,directory,&error);
    g_free(directory); g_free(config.port);
    if(!saved){
        GtkWidget *failure = gtk_message_dialog_new(win,GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,
            "Không lưu được cấu hình: %s",error->message);
        gtk_dialog_run(GTK_DIALOG(failure));gtk_widget_destroy(failure);g_error_free(error);
        g_free((gpointer)model);
        return;
    }
    // Restart ibus
    run_shell("ibus restart 2>/dev/null &");
    GtkWidget *dlg = gtk_message_dialog_new(win, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "Đã lưu %s, modern=%s, AI %s:%s. Đã restart ibus.", method, modern, ai_enabled, model);
    g_free((gpointer)model);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    gtk_main_quit();
}
static void on_cancel(GtkWidget *w, gpointer data){
    gtk_main_quit();
}
static gboolean update_progress(gpointer data){
    if(download_in_progress){
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_ai));
        return TRUE;
    }
    return FALSE;
}
static void on_download_clicked(GtkWidget *w, gpointer data){
    if(download_in_progress){
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Đang tải, vui lòng đợi...");
        return;
    }
    const char *model = g_strdup(gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_model)));
    if(!model || strlen(model)==0) { g_free((gpointer)model); model = g_strdup("qwen2:0.5b"); }
    // Lấy model name trước " ("
    char model_name[64]; strncpy(model_name, model, 63); model_name[63]=0;
    char *sp = strchr(model_name, ' '); if(sp) *sp=0;
    g_free((gpointer)model);
    // Đảm bảo ollama serve đang chạy (tự start nếu chưa)
    int port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_port));
    if(!ollama_serving(port)){
        ensure_ollama_serve();
        gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Đang khởi động Ollama serve — đợi vài giây rồi bấm Tải Model lại.");
        return;
    }
    download_in_progress = TRUE;
    gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Đang tải model, vui lòng đợi (có thể mất vài phút)...");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_ai), 0.0);
    g_timeout_add(100, update_progress, NULL);
    char pull_url[512];
    snprintf(pull_url, sizeof(pull_url), "http://localhost:%d/api/pull", port);
    char json[256];
    snprintf(json, sizeof(json), "{\"name\":\"%s\"}", model_name);
    char curl_cmd[1024];
    snprintf(curl_cmd, sizeof(curl_cmd), "curl -s -X POST %s -d '%s' -H 'Content-Type: application/json' > /tmp/ollama_pull.log 2>&1 &", pull_url, json);
    run_shell(curl_cmd);
    // Giả lập progress
    gtk_label_set_text(GTK_LABEL(lbl_ai_status), "Đã gửi yêu cầu tải, kiểm tra với: curl http://localhost:55602/api/tags");
    download_in_progress = FALSE;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_ai), 1.0);
    GtkWidget *dlg2 = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "Đã gửi yêu cầu tải %s qua port %d.\nKiểm tra tiến trình: watch -n1 'curl -s http://localhost:%d/api/tags | grep %s'", model_name, port, port, model_name);
    gtk_dialog_run(GTK_DIALOG(dlg2));
    gtk_widget_destroy(dlg2);
}
int setup_ui(int argc, char *argv[], const char *cur_method, const char *cur_modern, const char *cur_spell, const char *cur_ai_enable, const char *cur_model, const char *cur_url, const char *cur_port, const char *cfg){
    g_set_prgname("gotiengviet");
    g_set_application_name("GoTiengViet");
    config_path = strdup(cfg);
    gtk_init(&argc, &argv);

    GtkIconTheme *theme = gtk_icon_theme_get_default();
    if(theme){
        gtk_icon_theme_append_search_path(theme, "/usr/share/gotiengviet/icons");
        gtk_icon_theme_append_search_path(theme, "/usr/share/icons/hicolor");
    }
    gtk_window_set_default_icon_name("gotiengviet");

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "GoTiengViet Setup — github.com/isthaison/gotiengviet");
    gtk_window_set_default_size(GTK_WINDOW(win), 540, 620);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(win), "gotiengviet");
    gtk_window_set_icon_from_file(GTK_WINDOW(win), "/usr/share/gotiengviet/icons/gotiengviet.png", NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>GoTiengViet</b> — Gõ Telex/VNI thuần hệ thống\n<span size='small'>Chỉ dùng lib hệ thống (Go + gtk+-3.0), không lib ngoài</span>");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *f1 = gtk_frame_new("Kiểu gõ");
    gtk_box_pack_start(GTK_BOX(vbox), f1, FALSE, FALSE, 0);
    GtkWidget *box1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box1), 8);
    gtk_container_add(GTK_CONTAINER(f1), box1);
    rb_telex = gtk_radio_button_new_with_label(NULL, "Telex  — s f r x j, w z, aa ee oo aw ow uw dd, uow→ươ");
    rb_vni = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(rb_telex), "VNI     — 1 2 3 4 5, 6 7 8 9 0, a6→â, a8→ă, o7→ơ");
    if(strcmp(cur_method,"vni")==0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_vni), TRUE);
    else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_telex), TRUE);
    gtk_box_pack_start(GTK_BOX(box1), rb_telex, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box1), rb_vni, FALSE, FALSE, 0);

    GtkWidget *f2 = gtk_frame_new("Tùy chọn");
    gtk_box_pack_start(GTK_BOX(vbox), f2, FALSE, FALSE, 0);
    GtkWidget *box2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box2), 8);
    gtk_container_add(GTK_CONTAINER(f2), box2);
    cb_modern = gtk_check_button_new_with_label("Kiểu mới (modern) — hoà thay vì hòa cho oa/oe/uy (Bộ GD 2018)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_modern), strcmp(cur_modern,"true")==0);
    g_signal_connect(cb_modern, "toggled", G_CALLBACK(on_modern_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(box2), cb_modern, FALSE, FALSE, 0);
    lbl_preview = gtk_label_new(NULL);
    gtk_label_set_line_wrap(GTK_LABEL(lbl_preview), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_preview), 0.0);
    update_preview();
    gtk_box_pack_start(GTK_BOX(box2), lbl_preview, FALSE, FALSE, 0);
    cb_spell = gtk_check_button_new_with_label("Kiểm tra chính tả (chỉ gợi ý, không chặn)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_spell), strcmp(cur_spell,"true")==0);
    gtk_box_pack_start(GTK_BOX(box2), cb_spell, FALSE, FALSE, 0);
    GtkWidget *w2 = gtk_check_button_new_with_label("Cho phép w đơn -> ư (gõ w đầu từ ra ư)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w2), TRUE);
    gtk_widget_set_sensitive(w2, FALSE);
    gtk_box_pack_start(GTK_BOX(box2), w2, FALSE, FALSE, 0);

    // AI Frame
    GtkWidget *f_ai = gtk_frame_new("AI Local (Ollama) — Port 55602");
    gtk_box_pack_start(GTK_BOX(vbox), f_ai, FALSE, FALSE, 0);
    GtkWidget *box_ai = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box_ai), 8);
    gtk_container_add(GTK_CONTAINER(f_ai), box_ai);
    cb_ai = gtk_check_button_new_with_label("Bật AI gợi ý (tự chạy Ollama serve + hiện log bên dưới)");
    gtk_box_pack_start(GTK_BOX(box_ai), cb_ai, FALSE, FALSE, 0);
    g_signal_connect(cb_ai, "toggled", G_CALLBACK(on_ai_toggled), NULL);
    GtkWidget *hbox_ai = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(box_ai), hbox_ai, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_ai), gtk_label_new("Model:"), FALSE, FALSE, 0);
    combo_model = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_model), "qwen2:0.5b", "qwen2:0.5b (~400MB)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_model), "qwen2:1.5b", "qwen2:1.5b (~900MB)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_model), "rule", "rule (không model)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_model), 0);
    gtk_box_pack_start(GTK_BOX(hbox_ai), combo_model, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_ai), gtk_label_new("Port:"), FALSE, FALSE, 0);
    spin_port = gtk_spin_button_new_with_range(1024, 65535, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_port), 55602);
    gtk_widget_set_tooltip_text(spin_port, "Port Ollama, mặc định 55602 thay vì 11434");
    gtk_box_pack_start(GTK_BOX(hbox_ai), spin_port, FALSE, FALSE, 0);
    GtkWidget *hbox_url = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(box_ai), hbox_url, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_url), gtk_label_new("URL:"), FALSE, FALSE, 0);
    entry_url = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_url), "http://localhost:55602");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_url), "http://localhost:55602");
    gtk_box_pack_start(GTK_BOX(hbox_url), entry_url, TRUE, TRUE, 0);
    btn_download = gtk_button_new_with_label("Tải Model");
    g_signal_connect(btn_download, "clicked", G_CALLBACK(on_download_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_url), btn_download, FALSE, FALSE, 0);
    GtkWidget *btn_install = gtk_button_new_with_label("Cài Ollama");
    gtk_widget_set_tooltip_text(btn_install, "Tải và cài Ollama ngay trong app (hỏi mật khẩu root 1 lần), log hiện bên dưới");
    g_signal_connect(btn_install, "clicked", G_CALLBACK(on_install_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_url), btn_install, FALSE, FALSE, 0);
    progress_ai = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(box_ai), progress_ai, FALSE, FALSE, 0);
    lbl_ai_status = gtk_label_new("Chưa tải - bấm Tải Model để tải qwen2:0.5b qua Ollama 55602");
    gtk_label_set_line_wrap(GTK_LABEL(lbl_ai_status), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl_ai_status), 0.0);
    gtk_box_pack_start(GTK_BOX(box_ai), lbl_ai_status, FALSE, FALSE, 0);
    GtkWidget *lbl_log = gtk_label_new("Log Ollama serve (file /tmp/ollama_serve.log):");
    gtk_label_set_xalign(GTK_LABEL(lbl_log), 0.0);
    gtk_box_pack_start(GTK_BOX(box_ai), lbl_log, FALSE, FALSE, 0);
    log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(log_scroll, -1, 110);
    gtk_box_pack_start(GTK_BOX(box_ai), log_scroll, FALSE, FALSE, 0);
    log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(log_view), GTK_WRAP_WORD_CHAR);
    log_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));
    gtk_container_add(GTK_CONTAINER(log_scroll), log_view);
    // Khởi tạo AI từ config
    if(!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_model),cur_model)) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_model),cur_model,cur_model);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_model),cur_model);
    }
    gtk_entry_set_text(GTK_ENTRY(entry_url), cur_url);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_port), atoi(cur_port));
    g_signal_connect(spin_port,"value-changed",G_CALLBACK(port_changed),NULL);
    g_signal_connect(entry_url,"changed",G_CALLBACK(url_changed),NULL);
    url_changed(GTK_EDITABLE(entry_url),NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_ai), strcmp(cur_ai_enable,"true")==0);

    GtkWidget *info = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(info), "<span size='small'>Cấu hình lưu tại <tt>~/.config/gotiengviet/config</tt>\nGõ lại ký tự để xóa dấu: <tt>as-&gt;á, á s-&gt;a</tt>, <tt>uw-&gt;ư, ư w-&gt;u</tt></span>");
    gtk_label_set_line_wrap(GTK_LABEL(info), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), info, FALSE, FALSE, 0);

    GtkWidget *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(bbox, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
    GtkWidget *btn_cancel = gtk_button_new_with_label("Hủy");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel), NULL);
    gtk_box_pack_start(GTK_BOX(bbox), btn_cancel, FALSE, FALSE, 0);
    GtkWidget *btn_ok = gtk_button_new_with_label("Lưu & Đóng");
    GtkStyleContext *ctx = gtk_widget_get_style_context(btn_ok);
    gtk_style_context_add_class(ctx, "suggested-action");
    g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_save), win);
    gtk_box_pack_start(GTK_BOX(bbox), btn_ok, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}

#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

static void ensure_input_source(void) {
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (!source) return;
    GSettingsSchema *schema = g_settings_schema_source_lookup(source, "org.gnome.desktop.input-sources", TRUE);
    if (!schema) return;
    GSettings *settings = g_settings_new_full(schema, NULL, NULL);
    GVariant *current = g_settings_get_value(settings, "sources");
    GVariantIter iter;
    const gchar *kind, *name;
    gboolean found = FALSE;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));
    g_variant_iter_init(&iter, current);
    while (g_variant_iter_next(&iter, "(&s&s)", &kind, &name)) {
        if (!strcmp(kind, "ibus") && !strcmp(name, "gotiengviet")) found = TRUE;
        g_variant_builder_add(&builder, "(ss)", kind, name);
    }
    if (!found) {
        g_variant_builder_add(&builder, "(ss)", "ibus", "gotiengviet");
        g_settings_set_value(settings, "sources", g_variant_builder_end(&builder));
        g_settings_sync();
        g_spawn_command_line_async("ibus engine gotiengviet", NULL);
    } else g_variant_builder_clear(&builder);
    g_variant_unref(current);
    g_object_unref(settings);
    g_settings_schema_unref(schema);
}

static void cli_choice(const gchar *label, gchar *buffer, gsize size) {
    g_print("%s: ", label);
    if (!fgets(buffer, size, stdin)) buffer[0] = '\0';
    g_strstrip(buffer);
}

static int setup_cli(GtvConfig *config, const gchar *directory) {
    gchar input[256];
    g_print("GoTiengViet Setup (C) — %s, modern=%s, AI=%s\nEnter để giữ giá trị hiện tại.\n",
        config->mode == GTV_VNI ? "vni" : "telex", config->modern ? "true" : "false", config->ai_enabled ? "true" : "false");
    cli_choice("Kiểu gõ [telex/vni]", input, sizeof(input));
    if (!g_ascii_strcasecmp(input, "vni")) config->mode = GTV_VNI;
    if (!g_ascii_strcasecmp(input, "telex")) config->mode = GTV_TELEX;
    cli_choice("Modern [true/false]", input, sizeof(input));
    if (!g_ascii_strcasecmp(input, "true")) config->modern = TRUE;
    if (!g_ascii_strcasecmp(input, "false")) config->modern = FALSE;
    cli_choice("AI enable [true/false]", input, sizeof(input));
    if (!g_ascii_strcasecmp(input, "true")) config->ai_enabled = TRUE;
    if (!g_ascii_strcasecmp(input, "false")) config->ai_enabled = FALSE;
    cli_choice("AI port [1..65535]", input, sizeof(input));
    if (*input) {
        gchar *end;
        guint64 port = g_ascii_strtoull(input, &end, 10);
        if (*end || port < 1 || port > 65535) { g_printerr("Port không hợp lệ\n"); return 1; }
        g_free(config->port); config->port = g_strdup(input);
        g_free(config->url); config->url = g_strdup_printf("http://localhost:%s", input);
    }
    GError *error = NULL;
    if (!gtv_config_save(config, directory, &error)) {
        g_printerr("Không lưu được cấu hình: %s\n", error->message);
        g_error_free(error);
        return 1;
    }
    g_print("Đã lưu %s/config và ai.conf\n", directory);
    return 0;
}

int main(int argc, char **argv) {
    gboolean tray = FALSE, cli = FALSE;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--tray")) tray = TRUE;
        else if (!strcmp(argv[i], "--cli")) cli = TRUE;
        else if (!strcmp(argv[i], "--help")) {
            g_print("GoTiengViet Setup\n  --tray  Chạy indicator\n  --cli   Cấu hình qua terminal\n");
            return 0;
        } else { g_printerr("Tham số không hợp lệ: %s\n", argv[i]); return 1; }
    }
    gboolean graphical = g_getenv("DISPLAY") || g_getenv("WAYLAND_DISPLAY");
    if (tray) {
        if (!graphical) { g_printerr("Tray cần DISPLAY/WAYLAND_DISPLAY\n"); return 1; }
        gchar *lock_path = g_build_filename(g_get_user_runtime_dir(), "gotiengviet-tray.lock", NULL);
        int lock_fd = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        g_free(lock_path);
        if (lock_fd < 0) { g_printerr("Không mở được khóa tray\n"); return 1; }
        if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) { close(lock_fd); return 0; }
        ensure_input_source();
        int result = tray_run(argc, argv);
        close(lock_fd);
        return result;
    }
    gchar *directory = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    GtvConfig config;
    gtv_config_load(&config, directory);
    int result;
    if (cli || !graphical) result = setup_cli(&config, directory);
    else {
        ensure_input_source();
        gchar *self = g_file_read_link("/proc/self/exe", NULL);
        if (self) {
            gchar *tray_argv[] = {self, "--tray", NULL};
            g_spawn_async(NULL, tray_argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);
            g_free(self);
        }
        gchar *path = g_build_filename(directory, "config", NULL);
        result = setup_ui(argc, argv, config.mode == GTV_VNI ? "vni" : "telex",
            config.modern ? "true" : "false", config.spellcheck ? "true" : "false",
            config.ai_enabled ? "true" : "false", config.model, config.url, config.port, path);
        g_free(path);
    }
    gtv_config_clear(&config);
    g_free(directory);
    return result;
}
