#include "setup.h"
#include "app.h"
#include "data.h"
#include "tray.h"
#include "startup.h"
#include "ollama.h"
#include "resource.h"
#include "win_utf.h"
#include <glib/gstdio.h>

#define IDT_AI_LOG 1

/* Model dropdown (IDC_EDIT_MODEL is a CBS_DROPDOWN ComboBox, like the Linux
 * model combo): predefined IDs with size hints, custom IDs still typable. */
static const char *const ai_models[] = {
    "qwen2:0.5b (~400MB)",
    "qwen2:1.5b (~900MB)",
    "rule (không model)",
};

static void model_combo_add(HWND combo, const gchar *utf8) {
    glong wlen = 0;
    gunichar2 *w = g_utf8_to_utf16(utf8, -1, NULL, &wlen, NULL);
    if (w) {
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)w);
        g_free(w);
    }
}

/* Select the entry whose ID matches (labels after " (" are ignored, like
 * Linux current_model_id); unknown current IDs are appended as-is. */
static void model_combo_set(HWND hwnd, const gchar *current) {
    HWND combo = GetDlgItem(hwnd, IDC_EDIT_MODEL);
    if (!combo) return;
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (guint i = 0; i < G_N_ELEMENTS(ai_models); i++)
        model_combo_add(combo, ai_models[i]);
    gchar *id = g_strdup(current ? current : "");
    gchar *sp = strchr(id, ' ');
    if (sp) *sp = '\0';
    LRESULT idx = CB_ERR;
    if (*id) {
        glong wlen = 0;
        gunichar2 *w = g_utf8_to_utf16(id, -1, NULL, &wlen, NULL);
        if (w) {
            /* Match "qwen2:0.5b" against "qwen2:0.5b (~400MB)". */
            idx = SendMessageW(combo, CB_FINDSTRING, (WPARAM)-1, (LPARAM)w);
            g_free(w);
        }
        if (idx == CB_ERR) {
            model_combo_add(combo, id);
            idx = SendMessageW(combo, CB_GETCOUNT, 0, 0) - 1;
        }
    } else {
        idx = 0;
    }
    g_free(id);
    SendMessageW(combo, CB_SETCURSEL, (WPARAM)idx, 0);
}

/* Read the edit part and cut the display label (" (~400MB)"), like Linux. */
static gchar *model_combo_get(HWND hwnd) {
    gchar *text = gtv_win_get_dlg_item_text(hwnd, IDC_EDIT_MODEL, 512);
    if (text) {
        gchar *sp = strchr(text, ' ');
        if (sp) *sp = '\0';
        g_strstrip(text);
    }
    return text;
}
/* Tail state for %TEMP%\ollama_serve.log (Linux tails /tmp/ollama_serve.log
 * the same way: incremental offset, rotation reset, ~200-line cap). */
static long ai_log_off = 0;
static gchar *ai_log_seen = NULL;
static gboolean ai_log_primed = FALSE;

static void ai_log_append(HWND hwnd, const gchar *utf8) {
    if (!utf8 || !*utf8) return;
    HWND edit = GetDlgItem(hwnd, IDC_EDIT_AI_LOG);
    if (!edit) return;
    LRESULT len = SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0);
    if (len > 20000) {
        SendMessageW(edit, EM_SETSEL, 0, (LPARAM)(len - 20000));
        SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)L"");
    }
    SendMessageW(edit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    glong wlen = 0;
    gunichar2 *w = g_utf8_to_utf16(utf8, -1, NULL, &wlen, NULL);
    if (w) {
        SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)w);
        g_free(w);
    } else {
        SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)utf8);
    }
    /* Plain EDIT needs CRLF; without it all lines run together. */
    SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    SendMessageW(edit, EM_SCROLLCARET, 0, 0);
}

static void ai_log_poll(HWND hwnd, gboolean reset) {
    gchar *path = gtv_ollama_log_path();
    if (!ai_log_seen || g_strcmp0(ai_log_seen, path) != 0) {
        g_free(ai_log_seen);
        ai_log_seen = g_strdup(path);
        ai_log_off = 0;
        ai_log_primed = FALSE;
        reset = TRUE;
    }
    FILE *f = g_fopen(path, "rb");
    if (!f) {
        if (!ai_log_primed) {
            ai_log_append(hwnd, path);
            ai_log_append(hwnd, "(chưa có log — bật AI và lưu để tự khởi động ollama serve)");
            ai_log_primed = TRUE;
        }
        g_free(path);
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (reset) {
        ai_log_off = (sz > 8192) ? sz - 8192 : 0;
    } else if (sz < ai_log_off) {
        ai_log_off = 0; /* rotated/truncated */
    }
    fseek(f, ai_log_off, SEEK_SET);
    if (reset && ai_log_off > 0) {
        /* Skip a possible partial first line. */
        int c;
        while ((c = fgetc(f)) != EOF && c != '\n');
        ai_log_off = ftell(f);
    }
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0]) ai_log_append(hwnd, line);
    }
    ai_log_off = ftell(f);
    ai_log_primed = TRUE;
    fclose(f);
    g_free(path);
}

static BOOL CALLBACK SetChildFont(HWND child, LPARAM param) {
    SendMessageW(child, WM_SETFONT, (WPARAM)param, MAKELPARAM(TRUE, 0));
    return TRUE;
}

/* Pin a Vietnamese-capable dialog font at runtime. The template facename
 * depends on the resource compiler, and with DEFAULT_CHARSET the mapper
 * may fall back to a Latin-1-only face (U+0100+ renders as ?/tofu). Forcing
 * Segoe UI + VIETNAMESE_CHARSET keeps template size/weight and guarantees
 * ể/ặ/ố/... render on any system locale. Handle is freed on WM_DESTROY. */
static void pin_dialog_font(HWND hwnd) {
    HFONT current = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    LOGFONTW lf;
    if (!current || !GetObjectW(current, sizeof(lf), &lf))
        return;
    lstrcpynW(lf.lfFaceName, L"Segoe UI", LF_FACESIZE);
    lf.lfCharSet = VIETNAMESE_CHARSET;
    HFONT fixed = CreateFontIndirectW(&lf);
    if (!fixed)
        return;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)fixed);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)fixed, MAKELPARAM(TRUE, 0));
    EnumChildWindows(hwnd, SetChildFont, (LPARAM)fixed);
}

static INT_PTR CALLBACK SetupDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            /* Every visible string is set here in UTF-16 converted from
             * UTF-8: the .rc file carries ASCII placeholders only, so the
             * dialog renders Vietnamese correctly no matter which codepage
             * the resource compiler assumed. */
            struct { int id; const char *text; } labels[] = {
                { IDC_GROUP_TYPE, "Kiểu gõ" },
                { IDC_RADIO_TELEX, "Telex (gõ lặp: aa -> â, s/f/r/x/j)" },
                { IDC_RADIO_VNI, "VNI (số: 1->sắc, 2->huyền, 6->mũ...)" },
                { IDC_GROUP_OPTS, "Tùy chọn" },
                { IDC_CHECK_MODERN, "Đặt dấu theo chuẩn mới (oà, uỳ thay vì òa, ùy)" },
                { IDC_CHECK_SPELL, "Kiểm tra chính tả && Gợi ý từ thông minh" },
                { IDC_CHECK_STARTUP, "Khởi động cùng Windows" },
                { IDC_GROUP_AI, "Gợi ý AI (Ollama, tùy chọn)" },
                { IDC_CHECK_AI, "Bật gợi ý AI (cần Ollama + curl)" },
                { IDC_LBL_MODEL, "Model:" },
                { IDC_LBL_URL, "URL:" },
                { IDC_LBL_AI_STATUS, "Trạng thái Ollama..." },
                { IDC_LBL_AI_LOG, "Log Ollama serve:" },
                { IDC_BTN_OK, "Đồng ý" },
                { IDC_BTN_CANCEL, "Hủy" },
                { IDC_BTN_DATA, "Dữ liệu..." },
            };
            gtv_win_set_window_text(hwnd, "GoTiengViet - Cài đặt");
            for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
                gtv_win_set_dlg_item_text(hwnd, labels[i].id, labels[i].text);
            }
            pin_dialog_font(hwnd);
            if (g_app.config.mode == GTV_VNI) {
                CheckRadioButton(hwnd, IDC_RADIO_TELEX, IDC_RADIO_VNI, IDC_RADIO_VNI);
            } else {
                CheckRadioButton(hwnd, IDC_RADIO_TELEX, IDC_RADIO_VNI, IDC_RADIO_TELEX);
            }

            CheckDlgButton(hwnd, IDC_CHECK_MODERN, g_app.config.modern ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_SPELL, g_app.config.spellcheck ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_STARTUP, gtv_startup_get() != GTV_STARTUP_NONE ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_AI, g_app.config.ai_enabled ? BST_CHECKED : BST_UNCHECKED);
            model_combo_set(hwnd, g_app.config.model ? g_app.config.model : "");
            gtv_win_set_dlg_item_text(hwnd, IDC_EDIT_URL, g_app.config.url ? g_app.config.url : "");
            gtv_win_set_dlg_item_text(hwnd, IDC_LBL_AI_STATUS, "Đang kiểm tra Ollama...");
            gtv_win_set_dlg_item_text(hwnd, IDC_LBL_AI_LOG, "Log Ollama serve:");
            ai_log_off = 0;
            ai_log_primed = FALSE;
            g_free(ai_log_seen);
            ai_log_seen = NULL;
            ai_log_poll(hwnd, TRUE);
            SetTimer(hwnd, IDT_AI_LOG, 800, NULL);
            {
                gchar *url = gtv_win_get_dlg_item_text(hwnd, IDC_EDIT_URL, 512);
                gtv_ollama_probe_async(hwnd, url ? url : "");
                g_free(url);
            }

            /* Center dialog on screen */
            RECT rc, rcOwner;
            GetWindowRect(hwnd, &rc);
            GetWindowRect(GetDesktopWindow(), &rcOwner);
            SetWindowPos(hwnd, HWND_TOP,
                         (rcOwner.right - (rc.right - rc.left)) / 2,
                         (rcOwner.bottom - (rc.bottom - rc.top)) / 2,
                         0, 0, SWP_NOSIZE);
            return TRUE;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_OK) {
                if (IsDlgButtonChecked(hwnd, IDC_RADIO_VNI) == BST_CHECKED) {
                    g_app.config.mode = GTV_VNI;
                } else {
                    g_app.config.mode = GTV_TELEX;
                }
                g_app.config.modern = (IsDlgButtonChecked(hwnd, IDC_CHECK_MODERN) == BST_CHECKED);
                g_app.config.spellcheck = (IsDlgButtonChecked(hwnd, IDC_CHECK_SPELL) == BST_CHECKED);
                g_app.config.ai_enabled = (IsDlgButtonChecked(hwnd, IDC_CHECK_AI) == BST_CHECKED);

                /* String swap under lock: the AI worker may be copying them. */
                gtv_config_strings_lock();
                gchar *model = model_combo_get(hwnd);
                if (model && *g_strstrip(model)) {
                    g_free(g_app.config.model);
                    g_app.config.model = model;
                } else g_free(model);
                gchar *url = gtv_win_get_dlg_item_text(hwnd, IDC_EDIT_URL, 512);
                if (url && *g_strstrip(url)) {
                    g_free(g_app.config.url);
                    g_app.config.url = url;
                } else g_free(url);
                gtv_config_strings_unlock();

                gboolean want_startup = (IsDlgButtonChecked(hwnd, IDC_CHECK_STARTUP) == BST_CHECKED);
                if (want_startup) {
                    if (gtv_startup_get() == GTV_STARTUP_NONE)
                        gtv_startup_set_user(TRUE);
                } else if (gtv_startup_get() == GTV_STARTUP_USER) {
                    gtv_startup_set_user(FALSE);
                } else if (gtv_startup_get() == GTV_STARTUP_ADMIN) {
                    /* Removing the admin task needs elevation: UAC appears
                     * from Đồng ý. Cancelling it keeps the task. */
                    gtv_startup_request(hwnd, "admin-off");
                }

                gtv_app_save_config();
                if (g_app.config.ai_enabled)
                    gtv_ollama_ensure_all_async(hwnd, g_app.config.url, g_app.config.model);

                EndDialog(hwnd, IDOK);
                return TRUE;
            } else if (id == IDC_CHECK_AI && HIWORD(wParam) == BN_CLICKED) {
                if (IsDlgButtonChecked(hwnd, IDC_CHECK_AI) == BST_CHECKED) {
                    gtv_win_set_dlg_item_text(hwnd, IDC_LBL_AI_STATUS,
                        "Đang kiểm tra/cài Ollama (irm https://ollama.com/install.ps1 | iex)...");
                    gchar *url = gtv_win_get_dlg_item_text(hwnd, IDC_EDIT_URL, 512);
                    gchar *model = model_combo_get(hwnd);
                    gtv_ollama_ensure_all_async(hwnd, url ? url : "", model ? model : "");
                    g_free(url);
                    g_free(model);
                } else {
                    gtv_win_set_dlg_item_text(hwnd, IDC_LBL_AI_STATUS,
                        "AI tắt — dùng rule có sẵn, không cần Ollama.");
                }
                return TRUE;
            } else if (id == IDC_BTN_CANCEL || id == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            } else if (id == IDC_BTN_DATA) {
                gtv_data_show(hwnd);
                return TRUE;
            }
            break;
        }

        case WM_TIMER:
            if (wParam == IDT_AI_LOG) {
                ai_log_poll(hwnd, FALSE);
                return TRUE;
            }
            break;

        case WM_GTV_OLLAMA_STATUS:
            gtv_win_set_dlg_item_text(hwnd, IDC_LBL_AI_STATUS,
                wParam ? "Ollama đã chạy — sẵn sàng gợi ý."
                       : "Chưa kết nối được Ollama — xem log bên dưới.");
            return TRUE;

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;

        case WM_DESTROY: {
            KillTimer(hwnd, IDT_AI_LOG);
            HFONT fixed = (HFONT)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (fixed)
                DeleteObject(fixed);
            break;
        }
    }
    return FALSE;
}

void gtv_setup_show(HWND parent) {
    HINSTANCE hinst = GetModuleHandle(NULL);
    DialogBoxW(hinst, MAKEINTRESOURCEW(IDD_SETUP_DIALOG), parent, SetupDlgProc);
}
