#include "hook.h"
#include "tray.h"
#include "setup.h"
#include "update.h"
#include "resource.h"
#include <windows.h>
#include <glib.h>

GtvWindowsApp g_app = {0};

static const char *WINDOW_CLASS_NAME = "GoTiengViet_Message_Window";
static const char *MUTEX_NAME = "GoTiengViet_Single_Instance_Mutex";

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAY_CALLBACK: {
            if (lParam == WM_LBUTTONUP) {
                gtv_hook_toggle_mode();
            } else if (lParam == WM_RBUTTONUP) {
                gtv_tray_show_menu(hwnd);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                gtv_setup_show(hwnd);
            } else if (lParam == NIN_BALLOONUSERCLICK) {
                if (!gtv_update_balloon_clicked())
                    gtv_tray_apply_pending();
            }
            break;
        }
        case WM_GTV_AI_RESULT: {
            GtvAiResult *res = (GtvAiResult *)lParam;
            if (res) {
                gtv_tray_suggest_balloon(res->typed, res->fix);
                g_free(res->typed); g_free(res->fix); g_free(res);
            }
            break;
        }
        case WM_GTV_UPDATE_RESULT: {
            GtvUpdateResult *res = (GtvUpdateResult *)lParam;
            gtv_update_on_result(res, (gboolean)wParam);
            break;
        }
        case WM_GTV_UPDATE_DOWNLOADED: {
            gtv_update_on_downloaded((gchar *)lParam);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    /* 1. Single-instance check */
    HANDLE hMutex = CreateMutexA(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowA(WINDOW_CLASS_NAME, NULL);
        if (existing) {
            gtv_setup_show(existing);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    /* 2. Initialize GoTiengViet engine */
    gtv_init();

    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gtv_config_load(&g_app.config, config_dir);
    g_free(config_dir);

    g_app.engine = gtv_engine_new(&g_app.config);
    g_app.enabled = gtv_hook_load_enabled(TRUE); /* Restore V/E mode, default [V] */
    g_app.tsf_mode = gtv_hook_load_tsf_mode(TRUE); /* TSF owns keys unless user fell back to hook */

    /* 3. Register message window class */
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassExA(&wc);

    g_app.hwnd_main = CreateWindowExA(0, WINDOW_CLASS_NAME, "GoTiengViet Hidden Window",
                                      0, 0, 0, 0, 0,
                                      HWND_MESSAGE, NULL, hInstance, NULL);

    /* 4. Install Hook and Tray icon */
    if (!gtv_hook_install()) {
        gunichar2 *wmsg = g_utf8_to_utf16("Không thể cài đặt Hook bàn phím!", -1, NULL, NULL, NULL);
        gunichar2 *wtitle = g_utf8_to_utf16("GoTiengViet Lỗi", -1, NULL, NULL, NULL);
        MessageBoxW(NULL, (LPCWSTR)wmsg, (LPCWSTR)wtitle, MB_ICONERROR | MB_OK);
        g_free(wmsg);
        g_free(wtitle);
        return 1;
    }

    gtv_tray_init(g_app.hwnd_main);
    gtv_tray_update_icon(g_app.enabled);

    /* Daily self-update check against GitHub releases (throttled). */
    gtv_update_check_async(g_app.hwnd_main, FALSE);

    /* 5. Main Message Loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    /* 6. Cleanup */
    gtv_tray_cleanup();
    gtv_hook_uninstall();
    if (g_app.engine) {
        gtv_engine_free(g_app.engine);
        g_app.engine = NULL;
    }
    gtv_config_clear(&g_app.config);

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
