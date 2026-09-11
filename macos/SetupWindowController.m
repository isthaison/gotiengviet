#import "SetupWindowController.h"
#import <Cocoa/Cocoa.h>

/* Placeholder: bring the app forward so the user sees something while the
 * real preferences window (method / tone placement / spellcheck / Ollama)
 * is still WIP. Keeps main.m linkable. */
void GoTiengVietShowSettings(void) {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}
