#ifndef GTV_SUGGEST_OVERLAY_H
#define GTV_SUGGEST_OVERLAY_H

#include <windows.h>

/* The overlay is created by the tray executable, never by the TSF DLL. */
BOOL gtv_suggest_overlay_init(HINSTANCE instance, HWND owner);
void gtv_suggest_overlay_cleanup(void);
BOOL gtv_suggest_overlay_handle_copydata(const COPYDATASTRUCT *copydata);

#endif /* GTV_SUGGEST_OVERLAY_H */
