#ifndef GTV_SUGGEST_IPC_H
#define GTV_SUGGEST_IPC_H

#include <windows.h>

/* Fixed WM_COPYDATA contract between the TSF host and gotiengviet.exe.
 * Candidate strings are UTF-8 and include their trailing NUL in the slot. */
#define GTV_SUGGEST_COPYDATA_ID 0x4754534FU /* 'GTSO' */
#define GTV_SUGGEST_IPC_VERSION 1U
#define GTV_SUGGEST_IPC_MAX_CANDIDATES 5U
#define GTV_SUGGEST_IPC_CANDIDATE_BYTES 256U

typedef enum {
    GTV_SUGGEST_IPC_HIDE = 1,
    GTV_SUGGEST_IPC_SHOW = 2
} GtvSuggestIpcCommand;

typedef struct {
    DWORD version;
    DWORD command;
    DWORD source_pid;
    DWORD generation;
    RECT caret_screen;
    DWORD candidate_count;
    DWORD selected; /* highlighted row, < candidate_count */
    char candidates[GTV_SUGGEST_IPC_MAX_CANDIDATES]
                   [GTV_SUGGEST_IPC_CANDIDATE_BYTES];
} GtvSuggestIpcPayload;

#if defined(__cplusplus)
static_assert(sizeof(GtvSuggestIpcPayload) == 1320,
              "GtvSuggestIpcPayload must keep its fixed WM_COPYDATA layout");
#else
_Static_assert(sizeof(GtvSuggestIpcPayload) == 1320,
               "GtvSuggestIpcPayload must keep its fixed WM_COPYDATA layout");
#endif

#endif /* GTV_SUGGEST_IPC_H */
