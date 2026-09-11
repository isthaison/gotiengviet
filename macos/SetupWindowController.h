#ifndef GTV_SETUP_WINDOW_CONTROLLER_H
#define GTV_SETUP_WINDOW_CONTROLLER_H

/* macOS settings UI (WIP). main.m posts vn.gotiengviet.ShowSettings and
 * calls GoTiengVietShowSettings(); this stub keeps the target compiling
 * until the real preferences window lands. Mirrors the Linux setup
 * (linux/setup/main.c) and Windows dialog (windows/setup.c): input method,
 * tone placement, spellcheck, optional Ollama AI. */
void GoTiengVietShowSettings(void);

#endif /* GTV_SETUP_WINDOW_CONTROLLER_H */
