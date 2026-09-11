#include "data.h"
#include "resource.h"
#include "win_utf.h"
#include "internal.h"
#include <windows.h>
#include <shellapi.h>
#include <glib.h>

static gboolean show_emoji = FALSE;

static void list_refresh(HWND hwnd) {
    HWND list = GetDlgItem(hwnd, IDC_DATA_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    guint n = gtv_table_count(show_emoji);
    for (guint i = 0; i < n; i++) {
        const gchar *k = NULL, *v = NULL;
        if (!gtv_table_get(show_emoji, i, &k, &v)) continue;
        gchar *disp = g_strdup_printf("%s → %s", k, v);
        gunichar2 *w = g_utf8_to_utf16(disp, -1, NULL, NULL, NULL);
        if (w) {
            SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)w);
            g_free(w);
        }
        g_free(disp);
    }
}

static void fields_clear(HWND hwnd) {
    gtv_win_set_dlg_item_text(hwnd, IDC_DATA_KEY, "");
    gtv_win_set_dlg_item_text(hwnd, IDC_DATA_VALUE, "");
}

static void fields_from_selection(HWND hwnd) {
    HWND list = GetDlgItem(hwnd, IDC_DATA_LIST);
    LRESULT idx = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (idx < 0) return;
    const gchar *k = NULL, *v = NULL;
    if (!gtv_table_get(show_emoji, (guint)idx, &k, &v)) return;
    gtv_win_set_dlg_item_text(hwnd, IDC_DATA_KEY, k ? k : "");
    gtv_win_set_dlg_item_text(hwnd, IDC_DATA_VALUE, v ? v : "");
}

static void warn(HWND hwnd, const gchar *utf8) {
    gunichar2 *w = g_utf8_to_utf16(utf8, -1, NULL, NULL, NULL);
    gunichar2 *t = g_utf8_to_utf16("GoTiengViet", -1, NULL, NULL, NULL);
    if (w && t) MessageBoxW(hwnd, (LPCWSTR)w, (LPCWSTR)t, MB_OK | MB_ICONWARNING);
    g_free(w);
    g_free(t);
}

static void persist_or_warn(HWND hwnd) {
    GError *err = NULL;
    if (!gtv_table_save(show_emoji, &err)) {
        warn(hwnd, err ? err->message : "Không lưu được file dữ liệu.");
        g_clear_error(&err);
        return;
    }
    gtv_tables_reload();
}

static void on_save(HWND hwnd) {
    gchar *key = gtv_win_get_dlg_item_text(hwnd, IDC_DATA_KEY, 256);
    gchar *val = gtv_win_get_dlg_item_text(hwnd, IDC_DATA_VALUE, 1024);
    if (key) g_strstrip(key);
    if (val) g_strstrip(val);
    if (!gtv_table_set(show_emoji, key ? key : "", val ? val : "")) {
        warn(hwnd, "Khóa/giá trị không hợp lệ (khóa không chứa dấu cách hay dấu =, giá trị không rỗng).");
    } else {
        persist_or_warn(hwnd);
        list_refresh(hwnd);
    }
    g_free(key);
    g_free(val);
}

static void on_delete(HWND hwnd) {
    HWND list = GetDlgItem(hwnd, IDC_DATA_LIST);
    LRESULT idx = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (idx < 0) return;
    const gchar *k = NULL;
    if (!gtv_table_get(show_emoji, (guint)idx, &k, NULL) || !k) return;
    if (gtv_table_remove(show_emoji, k)) {
        persist_or_warn(hwnd);
        list_refresh(hwnd);
        fields_clear(hwnd);
    }
}

static void on_folder(void) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gunichar2 *w = g_utf8_to_utf16(dir ? dir : "", -1, NULL, NULL, NULL);
    if (w) {
        ShellExecuteW(NULL, L"open", (LPCWSTR)w, NULL, NULL, SW_SHOWNORMAL);
        g_free(w);
    }
    g_free(dir);
}

static INT_PTR CALLBACK DataDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            struct { int id; const char *text; } labels[] = {
                { IDC_DATA_MACRO, "Macro (gõ tắt)" },
                { IDC_DATA_EMOJI, "Emoji" },
                { IDC_LBL_KEY, "Từ gõ:" },
                { IDC_LBL_VALUE, "Nội dung:" },
                { IDC_DATA_SAVE, "Lưu" },
                { IDC_DATA_DELETE, "Xóa" },
                { IDC_DATA_FOLDER, "Mở thư mục" },
                { IDC_DATA_CLOSE, "Đóng" },
                { IDC_DATA_NOTE, "Thay đổi lưu vào file riêng của bạn. App đang mở phải khởi động lại mới nhận bảng mới." },
            };
            gtv_win_set_window_text(hwnd, "GoTiengViet - Dữ liệu");
            for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
                gtv_win_set_dlg_item_text(hwnd, labels[i].id, labels[i].text);
            }
            show_emoji = FALSE;
            CheckRadioButton(hwnd, IDC_DATA_MACRO, IDC_DATA_EMOJI, IDC_DATA_MACRO);
            SendMessageW(GetDlgItem(hwnd, IDC_DATA_KEY), EM_LIMITTEXT, 255, 0);
            SendMessageW(GetDlgItem(hwnd, IDC_DATA_VALUE), EM_LIMITTEXT, 1023, 0);
            list_refresh(hwnd);
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == IDC_DATA_MACRO || id == IDC_DATA_EMOJI) {
                gboolean emoji = (id == IDC_DATA_EMOJI);
                if (emoji != show_emoji) {
                    show_emoji = emoji;
                    list_refresh(hwnd);
                    fields_clear(hwnd);
                }
                return TRUE;
            }
            if (id == IDC_DATA_LIST && code == LBN_SELCHANGE) {
                fields_from_selection(hwnd);
                return TRUE;
            }
            if (id == IDC_DATA_SAVE) { on_save(hwnd); return TRUE; }
            if (id == IDC_DATA_DELETE) { on_delete(hwnd); return TRUE; }
            if (id == IDC_DATA_FOLDER) { on_folder(); return TRUE; }
            if (id == IDC_DATA_CLOSE || id == IDCANCEL) {
                EndDialog(hwnd, IDOK);
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void gtv_data_show(HWND parent) {
    HINSTANCE hinst = GetModuleHandle(NULL);
    DialogBoxW(hinst, MAKEINTRESOURCEW(IDD_DATA_DIALOG), parent, DataDlgProc);
}
