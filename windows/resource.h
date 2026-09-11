#ifndef RESOURCE_H
#define RESOURCE_H

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#define IDI_APP_ICON        101
#define IDI_TRAY_V          102
#define IDI_TRAY_E          103

#define IDD_SETUP_DIALOG    201
#define IDC_RADIO_TELEX     202
#define IDC_RADIO_VNI       203
#define IDC_CHECK_MODERN    204
#define IDC_CHECK_SPELL     205
#define IDC_CHECK_STARTUP   206
#define IDC_BTN_OK          207
#define IDC_BTN_CANCEL      208
#define IDC_CHECK_AI        209
#define IDC_EDIT_MODEL      210
#define IDC_EDIT_URL        211
/* Labels need individual IDs: every visible string is set at runtime
 * (UTF-8 -> UTF-16) so the dialog never depends on how windres decodes
 * non-ASCII resource text. */
#define IDC_GROUP_TYPE      212
#define IDC_GROUP_OPTS      213
#define IDC_GROUP_AI        214
#define IDC_LBL_MODEL       215
#define IDC_LBL_URL         216

#define ID_TRAY_SETTINGS    302
#define ID_TRAY_MODE_TELEX  303
#define ID_TRAY_MODE_VNI    304
#define ID_TRAY_SPELLCHECK  305
#define ID_TRAY_STARTUP     306
#define ID_TRAY_EXIT        307
#define ID_TRAY_MODERN      308
#define ID_TRAY_UPDATE      309
#define ID_TRAY_TOGGLE      311

#endif /* RESOURCE_H */
