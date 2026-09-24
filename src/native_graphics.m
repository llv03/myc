#import <AppKit/AppKit.h>
#include "native_graphics.h"
#include "report.h"

/* AppKit graphics for myc. Text (x,y) is top-left origin (flipped view).
 * Colors are floats in 0..1. Call graphics.run() to block until close. */

#define GFX_MAX_TEXT 256
#define GFX_STR_CAP  1024

typedef struct {
    double x, y;
    double r, g, b;
    char text[GFX_STR_CAP];
} GfxTextItem;

@interface MycGfxView : NSView
@end

@interface MycGfxDelegate : NSObject <NSWindowDelegate>
@end

static NSWindow *g_window = nil;
static MycGfxView *g_view = nil;
static MycGfxDelegate *g_delegate = nil;
static BOOL g_app_ready = NO;

static double g_bg_r = 1.0, g_bg_g = 1.0, g_bg_b = 1.0;
static double g_fg_r = 0.0, g_fg_g = 0.0, g_fg_b = 0.0;
static GfxTextItem g_items[GFX_MAX_TEXT];
static int g_item_count = 0;

@implementation MycGfxView
- (BOOL)isFlipped {
    return YES; /* top-left origin for myc users */
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[NSColor colorWithCalibratedRed:g_bg_r
                               green:g_bg_g
                                blue:g_bg_b
                               alpha:1.0] setFill];
    NSRectFill(self.bounds);

    NSFont *font = [NSFont systemFontOfSize:14.0];
    for (int i = 0; i < g_item_count; i++) {
        GfxTextItem *it = &g_items[i];
        NSColor *fg = [NSColor colorWithCalibratedRed:it->r
                                                green:it->g
                                                 blue:it->b
                                                alpha:1.0];
        NSDictionary *attrs = @{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: fg
        };
        NSString *s = [NSString stringWithUTF8String:it->text];
        if (!s) s = @"";
        [s drawAtPoint:NSMakePoint(it->x, it->y) withAttributes:attrs];
    }
}
@end

@implementation MycGfxDelegate
- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
    [NSApp stop:nil];
    /* Nudge the run loop so -stop takes effect immediately. */
    NSEvent *ev = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                     location:NSMakePoint(0, 0)
                                modifierFlags:0
                                    timestamp:0
                                 windowNumber:0
                                      context:nil
                                      subtype:0
                                        data1:0
                                        data2:0];
    [NSApp postEvent:ev atStart:YES];
}
@end

static void gfx_ensure_app(void) {
    if (g_app_ready) return;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    g_app_ready = YES;
}

static bool clamp01(Value v, double *out) {
    if (!is_number(v)) return false;
    double d = as_double(v);
    if (d < 0.0) d = 0.0;
    if (d > 1.0) d = 1.0;
    *out = d;
    return true;
}

static bool copy_title(Value title_v, char *buf, size_t n) {
    if (!is_string(title_v)) return false;
    ObjString *s = as_string(title_v);
    if ((size_t)s->length + 1 > n) return false;
    memcpy(buf, s->chars, (size_t)s->length);
    buf[s->length] = '\0';
    return true;
}

Value native_gfx_available(void) {
    return bool_val(true);
}

Value native_gfx_open(Value width_v, Value height_v, Value title_v) {
    if (!is_number(width_v) || !is_number(height_v)) return bool_val(false);
    char title[512];
    if (!copy_title(title_v, title, sizeof(title))) return bool_val(false);

    double w = as_double(width_v);
    double h = as_double(height_v);
    if (w < 1.0 || h < 1.0) return bool_val(false);

    gfx_ensure_app();

    if (g_window != nil) {
        [g_window close];
        g_window = nil;
        g_view = nil;
    }

    g_item_count = 0;
    g_bg_r = g_bg_g = g_bg_b = 1.0;
    g_fg_r = g_fg_g = g_fg_b = 0.0;

    NSRect content = NSMakeRect(0, 0, w, h);
    NSUInteger style = NSWindowStyleMaskTitled
                     | NSWindowStyleMaskClosable
                     | NSWindowStyleMaskMiniaturizable;
    g_window = [[NSWindow alloc] initWithContentRect:content
                                           styleMask:style
                                             backing:NSBackingStoreBuffered
                                               defer:NO];
    [g_window setTitle:[NSString stringWithUTF8String:title]];
    [g_window center];
    [g_window setReleasedWhenClosed:NO];

    g_view = [[MycGfxView alloc] initWithFrame:content];
    [g_window setContentView:g_view];

    if (g_delegate == nil)
        g_delegate = [[MycGfxDelegate alloc] init];
    [g_window setDelegate:g_delegate];

    [g_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    return bool_val(true);
}

Value native_gfx_title(Value title_v) {
    if (g_window == nil) return bool_val(false);
    char title[512];
    if (!copy_title(title_v, title, sizeof(title))) return bool_val(false);
    [g_window setTitle:[NSString stringWithUTF8String:title]];
    return bool_val(true);
}

Value native_gfx_bg(Value r_v, Value g_v, Value b_v) {
    double r, g, b;
    if (!clamp01(r_v, &r) || !clamp01(g_v, &g) || !clamp01(b_v, &b))
        return bool_val(false);
    g_bg_r = r;
    g_bg_g = g;
    g_bg_b = b;
    if (g_view) [g_view setNeedsDisplay:YES];
    return bool_val(true);
}

Value native_gfx_color(Value r_v, Value g_v, Value b_v) {
    double r, g, b;
    if (!clamp01(r_v, &r) || !clamp01(g_v, &g) || !clamp01(b_v, &b))
        return bool_val(false);
    g_fg_r = r;
    g_fg_g = g;
    g_fg_b = b;
    return bool_val(true);
}

Value native_gfx_text(Value x_v, Value y_v, Value s_v) {
    if (!is_number(x_v) || !is_number(y_v) || !is_string(s_v))
        return bool_val(false);
    if (g_item_count >= GFX_MAX_TEXT) return bool_val(false);

    ObjString *s = as_string(s_v);
    if (s->length >= GFX_STR_CAP) return bool_val(false);

    GfxTextItem *it = &g_items[g_item_count++];
    it->x = as_double(x_v);
    it->y = as_double(y_v);
    it->r = g_fg_r;
    it->g = g_fg_g;
    it->b = g_fg_b;
    memcpy(it->text, s->chars, (size_t)s->length);
    it->text[s->length] = '\0';

    if (g_view) [g_view setNeedsDisplay:YES];
    return bool_val(true);
}

Value native_gfx_run(void) {
    if (g_window == nil) return nil_val();
    gfx_ensure_app();
    [g_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
    g_window = nil;
    g_view = nil;
    return nil_val();
}
