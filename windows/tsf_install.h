#ifndef GTV_TSF_INSTALL_H
#define GTV_TSF_INSTALL_H

#include <glib.h>

void gtv_tsf_install_ensure_registered(void);

/* One-time machine registration: ITfInputProcessorProfiles::Register,
 * AddLanguageProfile (VI+EN), EnableLanguageProfile, plus
 * ITfCategoryMgr::RegisterCategory for the keyboard categories.
 * Needs admin (HKLM writes); returns TRUE when the profile enumerates
 * afterwards. Invoked as: gotiengviet.exe --register-tsf (elevated). */
gboolean gtv_tsf_register_elevated(void);

#endif /* GTV_TSF_INSTALL_H */
