#import "GoTiengVietInputController.h"
#include "engine.h"

@interface GoTiengVietInputController ()
{
    GtvEngine *engine;
    BOOL composing;
}
- (void)showBuffer:(id)sender;
- (void)commitBuffer:(id)sender;
- (void)clearComposition:(id)sender;
@end

@implementation GoTiengVietInputController

- (id)initWithServer:(id)server delegate:(id)delegate client:(id)inputClient
{
    self = [super initWithServer:server delegate:delegate client:inputClient];
    if (self) {
        gchar *config_dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
        GtvConfig config;
        gtv_config_load(&config, config_dir);
        g_free(config_dir);
        engine = gtv_engine_new(&config);
        gtv_config_clear(&config);
        composing = NO;
    }
    return self;
}

- (void)dealloc
{
    if (engine) {
        gtv_engine_free(engine);
        engine = NULL;
    }
    [super dealloc];
}

- (void)activateServer:(id)sender
{
    /* Fresh state on every activation; also picks up settings saved by
     * the preferences window (same files the Linux build uses). */
    if (engine) {
        gchar *config_dir = g_build_filename(g_get_user_config_dir(), "gotiengviet", NULL);
        GtvConfig config;
        gtv_config_load(&config, config_dir);
        g_free(config_dir);
        engine->mode = config.mode;
        engine->modern = config.modern;
        engine->spellcheck = config.spellcheck;
        gtv_config_clear(&config);
        gtv_engine_reset(engine);
    }
    composing = NO;
    [super activateServer:sender];
}

- (void)deactivateServer:(id)sender
{
    /* Commit anything left composing so no keystroke is ever lost. */
    if (composing) {
        [self commitBuffer:sender];
    }
    [super deactivateServer:sender];
}

- (NSString *)utf8String:(const gchar *)text
{
    if (!text || !*text) return nil;
    return [[[NSString alloc] initWithBytes:text
                                     length:strlen(text)
                                   encoding:NSUTF8StringEncoding] autorelease];
}

/* Render the engine buffer as inline (marked) text. Composed Vietnamese
 * is always BMP, so -length (UTF-16 units) is a safe caret offset here. */
- (void)showBuffer:(id)sender
{
    gchar *buf = gtv_engine_buffer(engine);
    if (!buf || !*buf) {
        g_free(buf);
        [self clearComposition:sender];
        return;
    }
    NSString *ns = [self utf8String:buf];
    g_free(buf);
    if (!ns) {
        [self clearComposition:sender];
        return;
    }
    composing = YES;
    [sender setMarkedText:ns
           selectionRange:NSMakeRange([ns length], 0)
         replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
}

- (void)commitBuffer:(id)sender
{
    gchar *buf = gtv_engine_buffer(engine);
    if (buf && *buf) {
        NSString *ns = [self utf8String:buf];
        if (ns) {
            [sender insertText:ns replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
        }
    }
    g_free(buf);
    gtv_engine_reset(engine);
    composing = NO;
}

- (void)clearComposition:(id)sender
{
    gtv_engine_reset(engine);
    composing = NO;
    [sender setMarkedText:@""
           selectionRange:NSMakeRange(0, 0)
         replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
}

/* If the client ate our marked text behind our back (focus dance, undo),
 * drop the stale buffer instead of prepending ghosts to the next commit. */
- (void)reconcileMarkedText:(id)sender
{
    if (!composing) return;
    @try {
        NSRange mr = [sender markedRange];
        if (mr.length == 0) {
            gtv_engine_reset(engine);
            composing = NO;
        }
    } @catch (NSException *e) {
        (void)e;
    }
}

- (BOOL)inputText:(id)string client:(id)sender
{
    if ([string isKindOfClass:[NSAttributedString class]]) {
        string = [string string];
    }
    if (![string isKindOfClass:[NSString class]] || [(NSString *)string length] == 0) {
        return NO;
    }
    NSString *text = (NSString *)string;

    /* Control characters (including delete) travel through
     * didCommandBySelector: instead; never double-handle them here. */
    if ([text length] == 1) {
        unichar c = [text characterAtIndex:0];
        if (c < 0x20 || (c >= 0xD800 && c <= 0xDFFF)) {
            return NO;
        }
    }

    /* Multi-char input (paste, emoji picker): flush any composition and
     * let the text insert itself. */
    if ([text length] != 1) {
        if (composing) {
            [self commitBuffer:sender];
        }
        return NO;
    }

    [self reconcileMarkedText:sender];

    const char *utf8 = [text UTF8String];
    if (!utf8 || !*utf8) return NO;
    gunichar ch = g_utf8_get_char_validated(utf8, -1);
    if (ch == (gunichar)-1 || ch == (gunichar)-2 || ch < 0x20) {
        return NO;
    }

    guint backspaces = 0;
    gchar *commit = gtv_engine_process(engine, ch, &backspaces);
    (void)backspaces; /* IMK edits via ranges; no Backspace injection needed. */
    if (commit) {
        NSString *ns = [self utf8String:commit];
        g_free(commit);
        if (ns) {
            [sender insertText:ns replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
        }
        gtv_engine_reset(engine);
        composing = NO;
        return YES;
    }

    [self showBuffer:sender];
    return YES;
}

- (BOOL)didCommandBySelector:(SEL)aSelector client:(id)sender
{
    NSString *sel = NSStringFromSelector(aSelector);

    /* Escape cancels the composition. */
    if ([sel isEqualToString:@"cancelOperation:"]) {
        if (composing) {
            [self clearComposition:sender];
            return YES;
        }
        return NO;
    }

    /* Backward deletes edit the composition; otherwise the app deletes. */
    if ([sel isEqualToString:@"deleteBackward:"] ||
        [sel isEqualToString:@"deleteWordBackward:"] ||
        [sel isEqualToString:@"deleteToBeginningOfLine:"]) {
        if (!composing) return NO;
        guint backspaces = 0;
        gtv_engine_process(engine, '\b', &backspaces);
        [self showBuffer:sender]; /* empties and ends tracking when done */
        return YES;
    }

    /* Anything else that touches text (newline, tab, arrows, forward
     * delete, insert commands): commit first, then let it through. */
    if ([sel hasPrefix:@"insert"] || [sel hasPrefix:@"delete"] ||
        [sel hasPrefix:@"move"] || [sel hasPrefix:@"transpose"] ||
        [sel hasPrefix:@"indent"] || [sel hasPrefix:@"change"]) {
        if (composing) {
            [self commitBuffer:sender];
        }
    }
    return NO;
}

@end
