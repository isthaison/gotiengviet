#include "app.h"
#include "tray.h"
#include "tray_ai.h"
#include "setup.h"
#include "startup.h"
#include "tsf_install.h"
#include "update.h"
#include "resource.h"
#include <windows.h>
#include <commctrl.h>
#include <glib.h>

GtvWindowsApp g_app = {0};

static const char *WINDOW_CLASS_NAME = GTV_TRAY_WINDOW_CLASS;
static const char *MUTEX_NAME = "GoTiengViet_Single_Instance_Mutex";

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAY_CALLBACK: {
            if (lParam == WM_LBUTTONUP) {
                gtv_app_toggle_mode();
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
        case WM_COPYDATA: {
            /* Committed word from gtv_tsf.dll for the AI typo check. */
            PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
            if (pcds && pcds->dwData == GTV_AI_COPYDATA_ID && pcds->cbData > 1
                && pcds->cbData <= 256 && pcds->lpData
                && ((const char *)pcds->lpData)[pcds->cbData - 1] == '\0') {
                gchar *word = g_strndup((const char *)pcds->lpData, pcds->cbData - 1);
                if (g_utf8_validate(word, -1, NULL)) gtv_tray_ai_word(word);
                else g_free(word);
            }
            return TRUE;
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
    (void)nCmdShow;

    /* Elevated startup worker: apply and exit before any window/mutex.
     * Forms: --configure-startup admin|admin-off|user (from gtv_startup_request). */
    if (lpCmdLine && !strncmp(lpCmdLine, "--configure-startup", 19)) {
        const char *op = lpCmdLine + 19;
        while (*op == ' ') op++;
        return gtv_startup_apply(op);
    }
    (void)lpCmdLine;

    /* Themed common controls (listview etc.) need explicit init. */
    {
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icc);
    }

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

    /* 2. Ensure this installation owns the per-user TSF registration. */
    gtv_tsf_install_ensure_registered();

    /* 3. Initialize GoTiengViet config */
    gtv_init();

    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    gtv_config_load(&g_app.config, config_dir);
    g_free(config_dir);

    g_app.enabled = gtv_app_load_enabled(TRUE); /* Restore V/E mode, default [V] */

    /* 4. Register message window class */
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassExA(&wc);

    g_app.hwnd_main = CreateWindowExA(0, WINDOW_CLASS_NAME, "GoTiengViet Hidden Window",
                                      0, 0, 0, 0, 0,
                                      HWND_MESSAGE, NULL, hInstance, NULL);

    /* 5. Initialize Tray Icon (typing is handled natively by Windows TSF) */
    gtv_tray_init(g_app.hwnd_main);

    /* Daily self-update check against GitHub releases (throttled). */
    gtv_update_check_async(g_app.hwnd_main, FALSE);

    /* 6. Main Message Loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    /* 7. Cleanup */
    gtv_tray_cleanup();
    gtv_config_clear(&g_app.config);

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
