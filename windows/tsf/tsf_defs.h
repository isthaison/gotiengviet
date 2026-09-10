#ifndef GTV_TSF_DEFS_H
#define GTV_TSF_DEFS_H

#include <windows.h>
#include <msctf.h>
#include <initguid.h>

/* GoTiengViet TSF Text Service CLSID:
 * {E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B} */
DEFINE_GUID(CLSID_GtvTextService,
    0xe3b0c442, 0x98fc, 0x4f2e, 0x9c, 0x8f, 0x7b, 0x2a, 0x3e, 0x1d, 0x4c, 0x5b);

/* GoTiengViet Language Profile GUID:
 * {D4C5B6A7-1E2F-4A3B-8C9D-0E1F2A3B4C5D} */
DEFINE_GUID(GUID_GtvProfile,
    0xd4c5b6a7, 0x1e2f, 0x4a3b, 0x8c, 0x9d, 0x0e, 0x1f, 0x2a, 0x3b, 0x4c, 0x5d);

#define GTV_TSF_MODEL_NAME L"GoTV"
#define GTV_TSF_DESC       L"GoTV"

/* Vietnamese Language ID: 0x042A */
#define GTV_LANG_VIETNAMESE MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT)
/* English (US) Language ID: 0x0409 (standard for systems without VN language pack) */
#define GTV_LANG_ENGLISH    MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)

#endif /* GTV_TSF_DEFS_H */
