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
            if (g_app.config.mode == GTV_VNI) {
                CheckRadioButton(hwnd, IDC_RADIO_TELEX, IDC_RADIO_VNI, IDC_RADIO_VNI);
            } else {
                CheckRadioButton(hwnd, IDC_RADIO_TELEX, IDC_RADIO_VNI, IDC_RADIO_TELEX);
            }

            CheckDlgButton(hwnd, IDC_CHECK_MODERN, g_app.config.modern ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_SPELL, g_app.config.spellcheck ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_STARTUP, get_startup_enabled_local() ? BST_CHECKED : BST_UNCHECKED);

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
