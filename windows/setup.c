#include "setup.h"
#include "hook.h"
#include "tray.h"
#include "resource.h"

extern void gtv_tray_set_startup(gboolean enable);

static gboolean get_startup_enabled_local(void) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return FALSE;
    char path[MAX_PATH];
    DWORD size = sizeof(path);
    LONG res = RegQueryValueEx(hkey, "GoTiengViet", NULL, NULL, (LPBYTE)path, &size);
    RegCloseKey(hkey);
    return res == ERROR_SUCCESS;
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
            };
            {
                gunichar2 *wcap = g_utf8_to_utf16("GoTiengViet - Cài đặt", -1, NULL, NULL, NULL);
                if (wcap) {
                    SetWindowTextW(hwnd, (LPCWSTR)wcap);
                    g_free(wcap);
                }
            }
            for (guint i = 0; i < G_N_ELEMENTS(labels); i++) {
                gunichar2 *w = g_utf8_to_utf16(labels[i].text, -1, NULL, NULL, NULL);
                if (w) {
                    SetDlgItemTextW(hwnd, labels[i].id, (LPCWSTR)w);
                    g_free(w);
                }
            }
            if (g_app.config.mode == GTV_VNI) {
                CheckRadioButton(hwnd, IDC_RADIO_TELEX, IDC_RADIO_VNI, IDC_RADIO_VNI);
            } else {
                CheckRadioButton(hwnd, IDC_RADIO_TELEX, IDC_RADIO_VNI, IDC_RADIO_TELEX);
            }

            CheckDlgButton(hwnd, IDC_CHECK_MODERN, g_app.config.modern ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_SPELL, g_app.config.spellcheck ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_STARTUP, get_startup_enabled_local() ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_AI, g_app.config.ai_enabled ? BST_CHECKED : BST_UNCHECKED);
            {
                /* Edit boxes are Unicode: convert, like every other UI string. */
                gunichar2 *wmodel = g_utf8_to_utf16(g_app.config.model ? g_app.config.model : "", -1, NULL, NULL, NULL);
                gunichar2 *wurl = g_utf8_to_utf16(g_app.config.url ? g_app.config.url : "", -1, NULL, NULL, NULL);
                SetDlgItemTextW(hwnd, IDC_EDIT_MODEL, (LPCWSTR)wmodel);
                SetDlgItemTextW(hwnd, IDC_EDIT_URL, (LPCWSTR)wurl);
                g_free(wmodel);
                g_free(wurl);
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
                WCHAR wbuf[512];
                gtv_config_strings_lock();
                GetDlgItemTextW(hwnd, IDC_EDIT_MODEL, wbuf, 512);
                gchar *model = g_utf16_to_utf8(wbuf, -1, NULL, NULL, NULL);
                if (model && *g_strstrip(model)) {
                    g_free(g_app.config.model);
                    g_app.config.model = model;
                } else g_free(model);
                GetDlgItemTextW(hwnd, IDC_EDIT_URL, wbuf, 512);
                gchar *url = g_utf16_to_utf8(wbuf, -1, NULL, NULL, NULL);
                if (url && *g_strstrip(url)) {
                    g_free(g_app.config.url);
                    g_app.config.url = url;
                } else g_free(url);
                gtv_config_strings_unlock();

                gboolean want_startup = (IsDlgButtonChecked(hwnd, IDC_CHECK_STARTUP) == BST_CHECKED);
                gtv_tray_set_startup(want_startup);

                if (g_app.engine) {
                    g_app.engine->mode = g_app.config.mode;
                    g_app.engine->modern = g_app.config.modern;
                    g_app.engine->spellcheck = g_app.config.spellcheck;
                }

                gchar *config_dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
                gtv_config_save(&g_app.config, config_dir, NULL);
                g_free(config_dir);

                EndDialog(hwnd, IDOK);
                return TRUE;
            } else if (id == IDC_BTN_CANCEL || id == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
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

void gtv_setup_show(HWND parent) {
    HINSTANCE hinst = GetModuleHandle(NULL);
    DialogBox(hinst, MAKEINTRESOURCE(IDD_SETUP_DIALOG), parent, SetupDlgProc);
}
