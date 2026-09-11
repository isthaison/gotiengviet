#import "SetupWindowController.h"
#import <Cocoa/Cocoa.h>
#include "engine.h"
#include "internal.h"

/* Preview settings: a single Macro/Emoji data window (no xib, manual
 * retain like the rest of macos/). Engine table API is shared with the
 * Windows/Linux managers; files reload in each process at startup. */
@interface GtvDataWindow : NSObject <NSTableViewDataSource, NSTableViewDelegate> {
    NSWindow *window;
    NSPopUpButton *typePop;
    NSTableView *table;
    NSTextField *keyField;
    NSTextField *valField;
    BOOL emoji;
}
+ (instancetype)shared;
- (void)show;
- (void)refresh;
- (IBAction)typeChanged:(id)sender;
- (IBAction)addOrUpdate:(id)sender;
- (IBAction)removeSelected:(id)sender;
- (IBAction)openFolder:(id)sender;
- (BOOL)saveTables;
@end

@implementation GtvDataWindow

+ (instancetype)shared {
    static GtvDataWindow *shared = nil;
    if (!shared) shared = [[GtvDataWindow alloc] init];
    return shared;
}

- (void)show {
    if (!window) {
        NSRect frame = NSMakeRect(0, 0, 480, 440);
        window = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
        [window setTitle:@"GoTiengViet — Dữ liệu"];
        [window setReleasedWhenClosed:NO];
        NSView *view = [window contentView];

        typePop = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(12, 402, 170, 26) pullsDown:NO];
        [typePop addItemsWithTitles:[NSArray arrayWithObjects:@"Macro (gõ tắt)", @"Emoji", nil]];
        [typePop setTarget:self];
        [typePop setAction:@selector(typeChanged:)];
        [view addSubview:typePop];
        [typePop release];

        NSTextField *note = [NSTextField labelWithString:@"Lưu xong khởi động lại app để nhận bảng mới."];
        [note setFrame:NSMakeRect(190, 402, 278, 26)];
        [[note cell] setTruncatesLastVisibleLine:YES];
        [view addSubview:note];

        NSScrollView *scroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(12, 122, 456, 268)] autorelease];
        [scroll setHasVerticalScroller:YES];
        [scroll setBorderType:NSBezelBorder];
        table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
        NSTableColumn *ck = [[[NSTableColumn alloc] initWithIdentifier:@"key"] autorelease];
        [ck setWidth:150];
        [[ck headerCell] setStringValue:@"Từ gõ"];
        NSTableColumn *cv = [[[NSTableColumn alloc] initWithIdentifier:@"value"] autorelease];
        [[cv headerCell] setStringValue:@"Nội dung"];
        [table addTableColumn:ck];
        [table addTableColumn:cv];
        [table setDataSource:self];
        [table setDelegate:self];
        [table setAllowsMultipleSelection:NO];
        [scroll setDocumentView:table];
        [table release];
        [view addSubview:scroll];

        NSTextField *kl = [NSTextField labelWithString:@"Từ gõ:"];
        [kl setFrame:NSMakeRect(12, 88, 60, 22)];
        [view addSubview:kl];
        keyField = [[NSTextField alloc] initWithFrame:NSMakeRect(78, 86, 390, 26)];
        [view addSubview:keyField];
        [keyField release];

        NSTextField *vl = [NSTextField labelWithString:@"Nội dung:"];
        [vl setFrame:NSMakeRect(12, 56, 60, 22)];
        [view addSubview:vl];
        valField = [[NSTextField alloc] initWithFrame:NSMakeRect(78, 54, 390, 26)];
        [view addSubview:valField];
        [valField release];

        NSArray *titles = [NSArray arrayWithObjects:@"Thêm/Cập nhật", @"Xóa", @"Mở thư mục", nil];
        NSRect rects[3] = {
            NSMakeRect(12, 12, 110, 30),
            NSMakeRect(130, 12, 80, 30),
            NSMakeRect(218, 12, 120, 30),
        };
        SEL actions[3] = { @selector(addOrUpdate:), @selector(removeSelected:), @selector(openFolder:) };
        for (NSUInteger i = 0; i < 3; i++) {
            NSButton *b = [[[NSButton alloc] initWithFrame:rects[i]] autorelease];
            [b setTitle:[titles objectAtIndex:i]];
            [b setButtonType:NSMomentaryPushInButton];
            [b setBezelStyle:NSRoundedBezelStyle];
            [b setTarget:self];
            [b setAction:actions[i]];
            [view addSubview:b];
        }
    }
    [self refresh];
    [window center];
    [window makeKeyAndOrderFront:nil];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

- (void)refresh {
    [table reloadData];
}

- (IBAction)typeChanged:(id)sender {
    (void)sender;
    emoji = ([typePop indexOfSelectedItem] == 1);
    [keyField setStringValue:@""];
    [valField setStringValue:@""];
    [self refresh];
}

- (BOOL)saveTables {
    GError *err = NULL;
    if (!gtv_table_save(emoji, &err)) {
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
        [alert setMessageText:@"GoTiengViet"];
        NSString *msg = err && err->message
            ? [NSString stringWithUTF8String:err->message]
            : @"Không lưu được file dữ liệu.";
        [alert setInformativeText:(msg ? msg : @"Không lưu được file dữ liệu.")];
        [alert runModal];
        g_clear_error(&err);
        return NO;
    }
    gtv_tables_reload();
    return YES;
}

- (IBAction)addOrUpdate:(id)sender {
    (void)sender;
    NSString *ks = [[keyField stringValue] stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *vs = [[valField stringValue] stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (!gtv_table_set(emoji, [ks UTF8String], [vs UTF8String])) {
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];
        [alert setMessageText:@"GoTiengViet"];
        [alert setInformativeText:@"Khóa/giá trị không hợp lệ (khóa không chứa dấu cách hay dấu =, giá trị không rỗng)."];
        [alert runModal];
        return;
    }
    if ([self saveTables]) {
        [keyField setStringValue:@""];
        [valField setStringValue:@""];
        [self refresh];
    }
}

- (IBAction)removeSelected:(id)sender {
    (void)sender;
    NSInteger row = [table selectedRow];
    if (row < 0) return;
    const gchar *k = NULL;
    if (!gtv_table_get(emoji, (guint)row, &k, NULL) || !k) return;
    if (gtv_table_remove(emoji, k) && [self saveTables]) {
        [keyField setStringValue:@""];
        [valField setStringValue:@""];
        [self refresh];
    }
}

- (IBAction)openFolder:(id)sender {
    (void)sender;
    gchar *dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
    if (dir) {
        NSString *ns = [NSString stringWithUTF8String:dir];
        if (ns) [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:ns]];
        g_free(dir);
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tv {
    (void)tv;
    return (NSInteger)gtv_table_count(emoji);
}

- (id)tableView:(NSTableView *)tv objectValueForTableColumn:(NSTableColumn *)col row:(NSInteger)row {
    (void)tv;
    const gchar *k = NULL, *v = NULL;
    if (!gtv_table_get(emoji, (guint)row, &k, &v)) return @"";
    BOOL wantKey = [[col identifier] isEqualToString:@"key"];
    const gchar *s = wantKey ? k : v;
    NSString *ns = s ? [NSString stringWithUTF8String:s] : nil;
    return ns ? ns : @"";
}

- (void)tableViewSelectionDidChange:(NSNotification *)n {
    (void)n;
    NSInteger row = [table selectedRow];
    if (row < 0) return;
    const gchar *k = NULL, *v = NULL;
    if (!gtv_table_get(emoji, (guint)row, &k, &v)) return;
    if (k) [keyField setStringValue:[NSString stringWithUTF8String:k]];
    if (v) [valField setStringValue:[NSString stringWithUTF8String:v]];
}

@end

/* Placeholder settings entry: opens the data manager until the full
 * preferences window lands. */
void GoTiengVietShowSettings(void) {
    [[GtvDataWindow shared] show];
}
