#ifndef HOOK_H
#define HOOK_H

#include <windows.h>
#include <glib.h>
#include "../engine/engine.h"

#define GTV_HOOK_MAGIC 0x5449454EU /* 'TIEN' */

typedef struct {
    gboolean enabled;       /* TRUE = Vietnamese [V], FALSE = English [E] */
    GtvConfig config;
    GtvEngine *engine;
    HHOOK keyboard_hook;
    HWND hwnd_main;
} GtvWindowsApp;

extern GtvWindowsApp g_app;

gboolean gtv_hook_install(void);
void gtv_hook_uninstall(void);
void gtv_hook_toggle_mode(void);
void gtv_hook_set_mode(gboolean enabled);
void gtv_hook_reset_buffer(void);

#endif /* HOOK_H */
