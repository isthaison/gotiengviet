#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>
#import <dispatch/dispatch.h>
#include "engine.h"
#include "SetupWindowController.h"

static const char *kConnectionName = "GoTiengViet_Connection";

static gboolean mac_asset_match(const gchar *asset, const gchar *tagver, gpointer unused) {
    (void)unused;
    gchar *want = g_strdup_printf("gotiengviet-%s-macos.zip", tagver);
    gboolean ok = strcmp(asset, want) == 0;
    g_free(want);
    return ok;
}

static NSString *bundleVersion(void) {
    NSString *v = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"];
    return (v && [v length]) ? v : @"0.0.0";
}

static void showAvailableAlert(NSString *tag, NSString *url) {
    NSString *msg = [NSString stringWithFormat:
        @"Có bản mới %@. Tải về ngay? (Giải nén vào ~/Library/Input Methods rồi đăng xuất/đăng nhập lại.)",
        tag];
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:@"GoTiengViet cập nhật"];
    [alert setInformativeText:msg];
    [alert addButtonWithTitle:@"Tải về"];
    [alert addButtonWithTitle:@"Để sau"];
    if ([alert runModal] != NSAlertFirstButtonReturn) return;
    NSString *dest = [NSString stringWithFormat:@"/tmp/gotiengviet-%@-macos.zip", tag];
    if (gtv_update_download([url UTF8String], [dest UTF8String])) {
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:
            [NSArray arrayWithObject:[NSURL fileURLWithPath:dest]]];
    } else {
        NSAlert *err = [[[NSAlert alloc] init] autorelease];
        [err setMessageText:@"GoTiengViet cập nhật"];
        [err setInformativeText:@"Không tải được bản mới (cần mạng và curl)."];
        [err runModal];
    }
}

/* Network off the main thread, alerts back on it. Manual mode always
 * reports; auto mode is silent unless an update exists. */
static void checkForUpdates(BOOL manual) {
    if (!manual) {
        if (!gtv_update_should_autocheck()) return;
        gtv_update_mark_checked();
    }
    NSString *current = bundleVersion();
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @autoreleasepool {
            gchar *tag = NULL, *url = NULL;
            GtvUpdateStatus st = gtv_update_check_full(NULL, [current UTF8String],
                                                       mac_asset_match, NULL, &tag, &url);
        NSString *nstag = tag ? [NSString stringWithUTF8String:tag] : nil;
        NSString *nsurl = url ? [NSString stringWithUTF8String:url] : nil;
        g_free(tag);
        g_free(url);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (st == GTV_UPDATE_AVAILABLE && nstag && nsurl) {
                showAvailableAlert(nstag, nsurl);
            } else if (manual) {
                NSAlert *alert = [[[NSAlert alloc] init] autorelease];
                [alert setMessageText:@"GoTiengViet cập nhật"];
                [alert setInformativeText:(st == GTV_UPDATE_CURRENT)
                    ? [NSString stringWithFormat:@"Đang dùng bản mới nhất (%@).", current]
                    : @"Không kiểm tra được bản mới. Thử lại sau."];
                [alert runModal];
            }
        });
    });
}

void GoTiengVietCheckForUpdates(BOOL manual) {
    checkForUpdates(manual);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    gtv_init();

    /* Bundled tables live in Contents/Resources/data (packaged by make);
     * user files in ~/.config/gotiengviet win as usual. */
    NSString *res = [[NSBundle mainBundle] resourcePath];
    if (res) {
        setenv("GTV_DATA_DIR",
               [[res stringByAppendingPathComponent:@"data"] UTF8String],
               1);
    }

    /* Second launch opens Settings instead of doubling the agent. */
    NSString *bid = [[NSBundle mainBundle] bundleIdentifier];
    NSArray *running = [NSRunningApplications runningApplicationsWithBundleIdentifier:bid];
    if ([running count] > 1) {
        [[NSDistributedNotificationCenter defaultCenter]
            postNotificationName:@"vn.gotiengviet.ShowSettings" object:nil];
        return 0;
    }
    [[NSDistributedNotificationCenter defaultCenter]
        addObserverForName:@"vn.gotiengviet.ShowSettings" object:nil queue:nil
               usingBlock:^(NSNotification *note) {
                   (void)note;
                   dispatch_async(dispatch_get_main_queue(), ^{
                       GoTiengVietShowSettings();
                   });
               }];

    IMKServer *server = [[IMKServer alloc]
        initWithName:[NSString stringWithUTF8String:kConnectionName]
        bundleIdentifier:bid];
    (void)server;

    checkForUpdates(NO);

    [[NSApplication sharedApplication] run];
    return 0;
}
