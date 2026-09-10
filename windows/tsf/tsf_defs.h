#ifndef GTV_TSF_DEFS_H
#define GTV_TSF_DEFS_H

#include <windows.h>
#include <msctf.h>
#include <initguid.h>

/* GoTiengViet TSF Text Service CLSID (random v4, generated 2026-09-10):
 * {B6696545-9A29-4229-97AC-BC59B3191CC1} */
DEFINE_GUID(CLSID_GtvTextService,
    0xb6696545, 0x9a29, 0x4229, 0x97, 0xac, 0xbc, 0x59, 0xb3, 0x19, 0x1c, 0xc1);

/* GoTiengViet Language Profile GUID (random v4, generated 2026-09-10):
 * {2BE303C6-B2D8-4C83-9826-F2CBC61D5251} */
DEFINE_GUID(GUID_GtvProfile,
    0x2be303c6, 0xb2d8, 0x4c83, 0x98, 0x26, 0xf2, 0xcb, 0xc6, 0x1d, 0x52, 0x51);

#define GTV_TSF_MODEL_NAME L"GoTiengViet Text Service"
#define GTV_TSF_DESC       L"GoTiengViet Vietnamese IME (TSF)"

/* Vietnamese Language ID: 0x042A */
#define GTV_LANG_VIETNAMESE MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT)
/* English (US) Language ID: 0x0409 (standard for systems without VN language pack) */
#define GTV_LANG_ENGLISH    MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)

#endif /* GTV_TSF_DEFS_H */
