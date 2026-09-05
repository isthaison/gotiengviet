package main

/*
#cgo pkg-config: gtk+-3.0
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *rb_telex, *rb_vni, *cb_modern, *cb_spell;
static char *config_path;

static void on_save(GtkWidget *w, gpointer data){
    GtkWindow *win = GTK_WINDOW(data);
    const char *method = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb_vni)) ? "vni" : "telex";
    const char *modern = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_modern)) ? "true" : "false";
    const char *spell = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb_spell)) ? "true" : "false";
    // Ghi file ~/.config/gotiengviet/config
    char *dir = g_path_get_dirname(config_path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    FILE *f = fopen(config_path, "w");
    if(f){
        fprintf(f, "[input]\nmethod=%s\nmodern=%s\nspellcheck=%s\ncharset=unicode\n", method, modern, spell);
        fclose(f);
    }
    // Restart ibus
    system("ibus restart 2>/dev/null &");
    GtkWidget *dlg = gtk_message_dialog_new(win, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "Đã lưu %s, modern=%s. Đã restart ibus.", method, modern);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    gtk_main_quit();
}
static void on_cancel(GtkWidget *w, gpointer data){
    gtk_main_quit();
}
int setup_ui(int argc, char *argv[], const char *cur_method, const char *cur_modern, const char *cur_spell, const char *cfg){
    config_path = strdup(cfg);
    gtk_init(&argc, &argv);
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "GoTiengViet Setup — github.com/isthaison/gotiengviet");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 380);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
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
    gtk_box_pack_start(GTK_BOX(box2), cb_modern, FALSE, FALSE, 0);
    cb_spell = gtk_check_button_new_with_label("Kiểm tra chính tả (chỉ gợi ý, không chặn)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb_spell), strcmp(cur_spell,"true")==0);
    gtk_box_pack_start(GTK_BOX(box2), cb_spell, FALSE, FALSE, 0);
    GtkWidget *w2 = gtk_check_button_new_with_label("Cho phép w đơn -> ư (gõ w đầu từ ra ư)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w2), TRUE);
    gtk_widget_set_sensitive(w2, FALSE);
    gtk_box_pack_start(GTK_BOX(box2), w2, FALSE, FALSE, 0);

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
*/
import "C"
import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"unsafe"
)

func loadConfig() (method, modern, spell string) {
	method = "telex"
	modern = "true"
	spell = "true"
	home, _ := os.UserHomeDir()
	cfg := filepath.Join(home, ".config", "gotiengviet", "config")
	f, err := os.Open(cfg)
	if err != nil {
		return
	}
	defer f.Close()
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if strings.HasPrefix(line, "method=") {
			v := strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			if v == "vni" || v == "telex" {
				method = v
			}
		}
		if strings.HasPrefix(line, "modern=") {
			v := strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			if v == "true" || v == "false" {
				modern = v
			}
		}
		if strings.HasPrefix(line, "spellcheck=") {
			v := strings.TrimSpace(strings.SplitN(line, "=", 2)[1])
			if v == "true" || v == "false" {
				spell = v
			}
		}
	}
	return
}

func main() {
	method, modern, spell := loadConfig()
	home, _ := os.UserHomeDir()
	cfgPath := filepath.Join(home, ".config", "gotiengviet", "config")

	// CLI fallback khi không có DISPLAY/WAYLAND_DISPLAY (Settings gọi với Wayland)
	if os.Getenv("DISPLAY") == "" && os.Getenv("WAYLAND_DISPLAY") == "" {
		fmt.Printf("GoTiengViet Setup (CLI Go) - hiện tại: %s modern=%s\n", method, modern)
		fmt.Println("1) telex  2) vni")
		fmt.Printf("Chọn [telex/vni] (Enter giữ %s): ", method)
		var inp string
		fmt.Scanln(&inp)
		inp = strings.TrimSpace(strings.ToLower(inp))
		if inp == "telex" || inp == "vni" {
			method = inp
		}
		fmt.Printf("Modern? [true/false] (hiện %s): ", modern)
		fmt.Scanln(&inp)
		inp = strings.TrimSpace(strings.ToLower(inp))
		if inp == "true" || inp == "false" {
			modern = inp
		}
		os.MkdirAll(filepath.Dir(cfgPath), 0755)
		os.WriteFile(cfgPath, []byte(fmt.Sprintf("[input]\nmethod=%s\nmodern=%s\nspellcheck=%s\ncharset=unicode\n", method, modern, spell)), 0644)
		fmt.Printf("Đã lưu %s\n", cfgPath)
		return
	}

	// GUI via CGO gtk+-3.0, thuần Go + lib hệ thống, không dùng gotk3
	cMethod := C.CString(method)
	cModern := C.CString(modern)
	cSpell := C.CString(spell)
	cCfg := C.CString(cfgPath)
	defer C.free(unsafe.Pointer(cMethod))
	defer C.free(unsafe.Pointer(cModern))
	defer C.free(unsafe.Pointer(cSpell))
	defer C.free(unsafe.Pointer(cCfg))

	argc := C.int(len(os.Args))
	// gtk_init cần argv
	cArgv := make([]*C.char, len(os.Args)+1)
	for i, a := range os.Args {
		cArgv[i] = C.CString(a)
	}
	defer func() {
		for _, p := range cArgv {
			if p != nil {
				C.free(unsafe.Pointer(p))
			}
		}
	}()

	C.setup_ui(argc, &cArgv[0], cMethod, cModern, cSpell, cCfg)
}
