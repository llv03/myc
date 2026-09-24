#include "native_graphics.h"
#include "report.h"

/* Non-macOS: graphics natives exist so imports resolve, but do nothing. */

static void report_unavailable_once(void) {
    static int reported = 0;
    if (reported) return;
    reported = 1;
    report_error(NULL, 0, 0, REPORT_RUNTIME,
                 "graphics is only available on macOS");
}

Value native_gfx_available(void) {
    return bool_val(false);
}

Value native_gfx_open(Value width, Value height, Value title) {
    (void)width;
    (void)height;
    (void)title;
    report_unavailable_once();
    return bool_val(false);
}

Value native_gfx_title(Value title) {
    (void)title;
    report_unavailable_once();
    return bool_val(false);
}

Value native_gfx_bg(Value r, Value g, Value b) {
    (void)r;
    (void)g;
    (void)b;
    report_unavailable_once();
    return bool_val(false);
}

Value native_gfx_color(Value r, Value g, Value b) {
    (void)r;
    (void)g;
    (void)b;
    report_unavailable_once();
    return bool_val(false);
}

Value native_gfx_text(Value x, Value y, Value s) {
    (void)x;
    (void)y;
    (void)s;
    report_unavailable_once();
    return bool_val(false);
}

Value native_gfx_run(void) {
    report_unavailable_once();
    return nil_val();
}
