#ifndef HOOK_H
#define HOOK_H

#include <windows.h>
#include <glib.h>
#include "../engine/engine.h"

#define GTV_HOOK_MAGIC 0x5449454EU /* 'TIEN' */

typedef struct {
    gboolean enabled;       /* TRUE = Vietnamese [V], FALSE = English [E] */
    gboolean tsf_mode;      /* TRUE = TSF text service owns keys, hook passive */
    GtvConfig config;
    GtvEngine *engine;
    HHOOK keyboard_hook;
    HHOOK mouse_hook;
    HWND hwnd_main;
    DWORD last_input_tick;  /* GetTickCount at the last real keydown */
} GtvWindowsApp;

extern GtvWindowsApp g_app;

gboolean gtv_hook_install(void);
void gtv_hook_uninstall(void);
void gtv_hook_toggle_mode(void);
void gtv_hook_set_mode(gboolean enabled);
void gtv_hook_reset_buffer(void);
void gtv_hook_send_backspaces(int count);
void gtv_hook_send_text(const gchar *utf8);
/* Erase erase_count screen chars (browser-aware) then send text. */
void gtv_hook_replace_text(int erase_count, const gchar *utf8);
/* Persist/restore the V/E mode in [input]/enabled of the user config file.
 * Unknown keys are ignored by the shared engine, so Linux is unaffected. */
void gtv_hook_save_enabled(void);
gboolean gtv_hook_load_enabled(gboolean def);
/* TSF mode flag lives in the same user config file ([input]/tsf_mode);
 * unknown keys are ignored by the shared engine, so Linux is unaffected. */
void gtv_hook_save_tsf_mode(void);
gboolean gtv_hook_load_tsf_mode(gboolean def);

#endif /* HOOK_H */
