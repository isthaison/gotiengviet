#include "tray.h"
#include "hook.h"
#include "setup.h"
#include "resource.h"

static NOTIFYICONDATA nid = {0};
static HICON icon_v = NULL;
static HICON icon_e = NULL;

static gboolean get_startup_enabled(void) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hkey) != ERROR_SUCCESS)
        return FALSE;
    char path[MAX_PATH];
    DWORD size = sizeof(path);
    LONG res = RegQueryValueEx(hkey, "GoTiengViet", NULL, NULL, (LPBYTE)path, &size);
    RegCloseKey(hkey);
    return res == ERROR_SUCCESS;
}

void gtv_tray_set_startup(gboolean enable) {
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS)
        return;
    if (enable) {
        char path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        RegSetValueEx(hkey, "GoTiengViet", 0, REG_SZ, (const BYTE *)path, (DWORD)strlen(path) + 1);
    } else {
        RegDeleteValue(hkey, "GoTiengViet");
    }
    RegCloseKey(hkey);
}

gboolean gtv_tray_init(HWND hwnd) {
    HINSTANCE hinst = GetModuleHandle(NULL);
    icon_v = LoadIcon(hinst, MAKEINTRESOURCE(IDI_TRAY_V));
    icon_e = LoadIcon(hinst, MAKEINTRESOURCE(IDI_TRAY_E));

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_CALLBACK;
    nid.hIcon = g_app.enabled ? icon_v : icon_e;
    strcpy(nid.szTip, "GoTiengViet - Bộ gõ tiếng Việt");

    return Shell_NotifyIcon(NIM_ADD, &nid);
}

void gtv_tray_cleanup(void) {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (icon_v) DestroyIcon(icon_v);
    if (icon_e) DestroyIcon(icon_e);
}

void gtv_tray_update_icon(gboolean enabled) {
    nid.hIcon = enabled ? icon_v : icon_e;
    if (enabled) {
        strcpy(nid.szTip, "GoTiengViet [Tiếng Việt]");
    } else {
        strcpy(nid.szTip, "GoTiengViet [English]");
    }
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void gtv_tray_show_menu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hmenu = CreatePopupMenu();
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, "Bảng điều khiển...");
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

    UINT telex_flag = (g_app.config.mode == GTV_TELEX) ? MF_CHECKED : MF_UNCHECKED;
    UINT vni_flag = (g_app.config.mode == GTV_VNI) ? MF_CHECKED : MF_UNCHECKED;
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | telex_flag, ID_TRAY_MODE_TELEX, "Kiểu gõ Telex");
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | vni_flag, ID_TRAY_MODE_VNI, "Kiểu gõ VNI");

    UINT spell_flag = g_app.config.spellcheck ? MF_CHECKED : MF_UNCHECKED;
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | spell_flag, ID_TRAY_SPELLCHECK, "Kiểm tra chính tả");

    UINT start_flag = get_startup_enabled() ? MF_CHECKED : MF_UNCHECKED;
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING | start_flag, ID_TRAY_STARTUP, "Khởi động cùng Windows");

    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenu(hmenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, "Thoát");

    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(hmenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hmenu);

    switch (cmd) {
        case ID_TRAY_SETTINGS:
            gtv_setup_show(hwnd);
            break;
        case ID_TRAY_MODE_TELEX:
            g_app.config.mode = GTV_TELEX;
            if (g_app.engine) g_app.engine->mode = GTV_TELEX;
            gtv_config_save(&g_app.config, g_get_user_config_dir(), NULL);
            break;
        case ID_TRAY_MODE_VNI:
            g_app.config.mode = GTV_VNI;
            if (g_app.engine) g_app.engine->mode = GTV_VNI;
            gtv_config_save(&g_app.config, g_get_user_config_dir(), NULL);
            break;
        case ID_TRAY_SPELLCHECK:
            g_app.config.spellcheck = !g_app.config.spellcheck;
            if (g_app.engine) g_app.engine->spellcheck = g_app.config.spellcheck;
            gtv_config_save(&g_app.config, g_get_user_config_dir(), NULL);
            break;
        case ID_TRAY_STARTUP:
            gtv_tray_set_startup(!get_startup_enabled());
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
    }
}
