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
/* Persist/restore the V/E mode in [input]/enabled of the user config file.
 * Unknown keys are ignored by the shared engine, so Linux is unaffected. */
void gtv_hook_save_enabled(void);
gboolean gtv_hook_load_enabled(gboolean def);

#endif /* HOOK_H */
