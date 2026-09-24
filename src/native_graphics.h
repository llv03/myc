#ifndef MYC_NATIVE_GRAPHICS_H
#define MYC_NATIVE_GRAPHICS_H

#include "value.h"

/* Mac-only AppKit window + text (see native_graphics.m).
 * On other platforms the stub returns false/nil and reports once.
 * Colors are floats in 0..1. Text (x,y) uses top-left origin. */

Value native_gfx_available(void);                          /* → bool */
Value native_gfx_open(Value width, Value height, Value title); /* → bool */
Value native_gfx_title(Value title);                       /* → bool */
Value native_gfx_bg(Value r, Value g, Value b);            /* → bool */
Value native_gfx_color(Value r, Value g, Value b);         /* → bool */
Value native_gfx_text(Value x, Value y, Value s);          /* → bool */
Value native_gfx_run(void);                                /* → nil; blocks until close */

#endif
