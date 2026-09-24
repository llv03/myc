# Graphics (macOS AppKit)

Minimal native window: title, background color, draw text, run until closed.
Linux and other hosts link a stub: `available()` is false and other calls
report once: `graphics is only available on macOS`.

No buttons, images, mouse/keyboard events, or OpenGL.

## Build

On Darwin, `make` compiles `src/native_graphics.m` with clang as Objective-C
(`-fobjc-arc`), defines `MYC_HAS_GRAPHICS=1`, and links
`-framework AppKit -framework Foundation`. Elsewhere only
`src/native_graphics_stub.c` is built.

## API

```
import graphics;
if (graphics.available()) {
  graphics.open(640, 480, "myc");
  graphics.title("Hello");
  graphics.bg(0.95, 0.95, 0.95);
  graphics.color(0.1, 0.1, 0.1);
  graphics.text(24, 40, "Hello from myc");
  graphics.run();
}
```

| Function | Notes |
|----------|--------|
| `available()` | `true` on macOS with AppKit linked; else `false` |
| `open(w, h, title)` | create `NSWindow` + content view; bool |
| `title(s)` | set window title; bool |
| `bg(r, g, b)` | fill color floats 0..1; bool |
| `color(r, g, b)` | current text color floats 0..1; bool |
| `text(x, y, s)` | queue string at top-left `(x,y)`; bool |
| `run()` | `[NSApp run]` until title-bar close; blocking |

Wrappers in `builtins/graphics.my` call `__gfx_*` natives.

Colors are floats in `0..1`. Text `(x, y)` uses **top-left** origin of the
content area (converted internally from AppKit coordinates).

## Demo

Manual, Mac only. Do not call `run()` from `make test`.

```bash
make
./myc examples/graphics_demo.my
```

`examples/graphics_avail.my` prints `graphics.available()` (false on non-Mac).
