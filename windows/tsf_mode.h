#ifndef TSF_MODE_H
#define TSF_MODE_H

#include <windows.h>
#include <glib.h>

/* Enable or disable our TSF language profiles (English + Vietnamese).
 * Returns TRUE when both toggles succeeded. Needs the DLL registered
 * (installer regsvr32 step); fails cleanly on portable runs. */
gboolean gtv_tsf_set_enabled(gboolean on);

#endif /* TSF_MODE_H */
