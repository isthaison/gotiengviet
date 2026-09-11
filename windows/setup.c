#include "setup.h"
#include "app.h"
#include "data.h"
#include "tray.h"
#include "startup.h"
#include "resource.h"
#include "win_utf.h"

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
            gtv_win_set_dlg_item_text(hwnd, IDC_EDIT_MODEL, g_app.config.model ? g_app.config.model : "");
            gtv_win_set_dlg_item_text(hwnd, IDC_EDIT_URL, g_app.config.url ? g_app.config.url : "");

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
                gchar *model = gtv_win_get_dlg_item_text(hwnd, IDC_EDIT_MODEL, 512);
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

                EndDialog(hwnd, IDOK);
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

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;

        case WM_DESTROY: {
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
