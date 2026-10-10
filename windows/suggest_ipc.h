#ifndef GTV_SUGGEST_IPC_H
#define GTV_SUGGEST_IPC_H

#include <windows.h>

/* Fixed WM_COPYDATA contract between the TSF host and gotiengviet.exe.
 * Candidate strings are UTF-8 and include their trailing NUL in the slot.
 *
 * Version 2 adds caret_source: the TSF DLL says which probe produced the
 * rect, so a mispositioned popup can be attributed instead of guessed at.
 * Bumping the version keeps an old tray and a new DLL (or the reverse) from
 * silently mis-reading the payload -- both sides check `version` and `cbData`,
 * so a mismatch simply drops the message. */
#define GTV_SUGGEST_COPYDATA_ID 0x4754534FU /* 'GTSO' */
#define GTV_SUGGEST_IPC_VERSION 2U
#define GTV_SUGGEST_IPC_MAX_CANDIDATES 5U
#define GTV_SUGGEST_IPC_CANDIDATE_BYTES 256U

typedef enum {
    GTV_SUGGEST_IPC_HIDE = 1,
    GTV_SUGGEST_IPC_SHOW = 2
} GtvSuggestIpcCommand;

/* Which probe produced caret_screen. Diagnostic only: it tells us whether a
 * misplaced popup came from the real caret, the composition box, or a
 * fallback, which is the difference between "Chromium answered wrong" and
 * "our own fallback invented it". */
typedef enum {
    GTV_CARET_SRC_NONE = 0,
    GTV_CARET_SRC_SELECTION = 1, /* GetSelection + GetTextExt: the real caret */
    GTV_CARET_SRC_COMPOSITION = 2,/* composition range, collapsed to its right */
    GTV_CARET_SRC_WIN32 = 3,      /* GetGUIThreadInfo rcCaret */
    GTV_CARET_SRC_CACHED = 4      /* last successful rect of this composition */
} GTV_CARET_SOURCE;

typedef struct {
    DWORD version;
    DWORD command;
    DWORD source_pid;
    DWORD generation;
    RECT caret_screen;
    DWORD candidate_count;
    DWORD selected; /* highlighted row, < candidate_count */
    DWORD caret_source; /* GtvCaretSource, diagnostic */
    char candidates[GTV_SUGGEST_IPC_MAX_CANDIDATES]
                   [GTV_SUGGEST_IPC_CANDIDATE_BYTES];
} GtvSuggestIpcPayload;

#if defined(__cplusplus)
static_assert(sizeof(GtvSuggestIpcPayload) == 1324,
              "GtvSuggestIpcPayload must keep its fixed WM_COPYDATA layout");
#else
_Static_assert(sizeof(GtvSuggestIpcPayload) == 1324,
               "GtvSuggestIpcPayload must keep its fixed WM_COPYDATA layout");
#endif

#endif /* GTV_SUGGEST_IPC_H */
