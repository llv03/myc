# Stdlib

## VM builtins

Always available (no import):

| Builtin | Meaning |
|---------|---------|
| `out(x)` | print any value (also statement form `out expr;`) |
| `len(x)` | length of string / list / map |
| `str(x)` | convert to string |
| `type(x)` | type name as string (`"int"`, `"list"`, ...) |

Also registered for packages: `__map_keys(m)` -> list of keys.

## Packages overview

| Package | Kind | Role |
|---------|------|------|
| `builtins/math.my` | pure myc | `math.sqrt`, `math.sin`, ... |
| `builtins/net.my` | myc wrappers | TCP around C `__net_*` natives |
| `builtins/http.my` | pure myc + net | HTTP/1.1 parse / respond / serve |
| `builtins/fs.my` | myc wrappers | text files around C `__fs_*` natives |
| `builtins/os.my` | myc wrappers | argv, shell cmds, getenv/cwd |
| `builtins/json.my` | pure myc | `json.parse` / `json.stringify` |
| `builtins/csv.my` | pure myc + fs | CSV parse/stringify + `read`/`write` |
| `builtins/graphics.my` | myc wrappers | Mac AppKit window (stub elsewhere) |

Import by name: `import math;` then call `math.sqrt(9)`.

## math

```
import math;
out(math.sqrt(9));
out(math.sin(0));
out(math.pi());
```

| Function | Notes |
|----------|--------|
| `pi()` / `e()` | constants as zero-arg fns |
| `abs` `min` `max` `clamp` | |
| `floor` `ceil` `round` | via float `%` |
| `pow(base, exp)` | integer exponent (neg -> reciprocal) |
| `sqrt(x)` | Newton; `x < 0` -> `nil` |
| `sin` `cos` `tan` | Taylor + range-reduce |
| `hypot(a,b)` `lerp(a,b,t)` | |

## Native filesystem (`__fs_*`)

| Native | Meaning |
|--------|---------|
| `__fs_read(path)` | whole file -> string, or `nil` (cap 8 MiB) |
| `__fs_write(path, contents)` | create/overwrite -> `true`/`false` |
| `__fs_append(path, contents)` | append -> `true`/`false` |
| `__fs_exists(path)` | -> bool |
| `__fs_remove(path)` | unlink -> bool |

Paths are relative to the process cwd (or absolute). Running `./myc script.my`
from another directory changes where relative paths resolve (not the script
folder). See `src/native_fs.c`.

### fs wrappers

```
import fs;
fs.write("notes.txt", "hello");
fs.append("notes.txt", "\nmore");
out(fs.read("notes.txt"));
out(fs.exists("notes.txt"));
fs.remove("notes.txt");
```

| Function | Maps to |
|----------|---------|
| `fs.read` `fs.write` `fs.append` `fs.exists` `fs.remove` | `__fs_*` |

## Native process / CLI

| Native | Meaning |
|--------|---------|
| `__args()` | list of strings: `[0]` = script path, then user args |
| `__system(cmd)` | run via shell (`system()`); exit status int; -1 if launch fails |
| `__capture(cmd)` | map `{"status","out","err"}`, or `nil` on launch failure |
| `__getenv(name)` | string or `nil` |
| `__cwd()` | cwd string, or `nil` |

Myc flags (e.g. `-d`) are stripped before building the args list:

```bash
./myc -d examples/args_demo.my foo bar
# os.args() -> ["examples/args_demo.my", "foo", "bar"]
```

`__system` / `__capture` use `/bin/sh -c` (or `system()`). Untrusted strings
with shell metacharacters are an injection hazard; only pass trusted command
strings. Capture uses `fork` + pipes; stdout -> `"out"`, stderr -> `"err"`
(cap 8 MiB each). See `src/native_os.c`.

### os wrappers

```
import os;
out(os.args());
out(os.system("true"));
let r = os.capture("echo hi");
out(r["out"]);
out(r["status"]);
out(os.getenv("HOME"));
out(os.cwd());
```

## Native C sockets (`__net_*`)

| Native | Meaning |
|--------|---------|
| `__net_listen(port)` | bind `0.0.0.0:port`, listen -> fd int or `nil` |
| `__net_accept(server)` | blocking accept -> conn fd or `nil` |
| `__net_recv(conn, n)` | up to `n` bytes -> string (may be partial/`""`) |
| `__net_send(conn, s)` | send string -> bytes sent (int) or `nil` |
| `__net_close(id)` | close fd -> `true` or `nil` |

IDs are raw POSIX socket file descriptors (`src/native_net.c`). Blocking I/O.

### net wrappers

```
import net;
let s = net.listen(8080);
let c = net.accept(s);
let raw = net.recv(c, 4096);
net.send(c, "hi");
net.close(c);
net.close(s);
```

## http

Pure myc on top of `net`. HTTP/1.1 only.

```
import http;

http.mk_response(200, "text/plain", "hi");
http.text(200, "ok");
http.html(200, "<h1>hi</h1>");
http.json(200, "{\"ok\":true}");

let req = http.parse_request(raw);  // map: method, path, body, raw

fn handler(req) {
  ret http.html(200, "<h1>Hello from myc</h1>");
}
http.serve(8765, handler);

http.serve_routes(8765, {"/": "<h1>hi</h1>"});
http.serve_text(8765, "<h1>hi</h1>");
```

| Function | Notes |
|----------|--------|
| `mk_response(status, content_type, body)` | raw HTTP/1.1 response string |
| `text` `html` `json` | convenience content types |
| `parse_request(raw)` | map with `method`, `path`, `body`, `raw` |
| `serve(port, handler)` | handler is a function value |
| `serve_routes(port, map)` | path -> HTML/body string |
| `serve_text(port, body)` | single response for any path |

Manual demo:

```bash
./myc examples/http_demo.my
curl http://127.0.0.1:8765/
```

`make test` does not start a live server; it checks builders/parsers via
`examples/http_parse_test.my`.

## json

Practical subset (not full RFC JSON). Pure myc.

```
import json;
let s = json.stringify({"a": 1, "tags": ["x", "y"]});
let v = json.parse(s);
out(v["a"]);
```

| Function | Notes |
|----------|--------|
| `stringify(v)` | nil/bool/int/float/string/list/map; escapes `\\ " \n \t \r`; int map keys stringified; function/struct -> `nil` |
| `parse(text)` | objects->maps, arrays->lists, whole numbers->int else float, `null`->nil; fails -> `nil`. No `1e3` exponents. |

## csv

RFC4180-ish: commas, quoted fields, `""` escapes. Pure myc + `fs`.

```
import csv;
let rows = csv.parse("a,b\n1,\"2,3\"\n");
let maps = csv.parse_maps("name,age\nAda,36\n");
out(csv.stringify([["a","b"],["1","2"]]));
csv.write("out.csv", rows);
let again = csv.read("out.csv");
```

| Function | Notes |
|----------|--------|
| `parse(text)` | list of rows (each a list of string fields) |
| `parse_maps(text)` | first row = headers -> list of maps |
| `stringify(rows)` | CSV text |
| `read(path)` / `write(path, rows)` | via `fs` |

## graphics

See [graphics.md](graphics.md). Short API:

| Function | Notes |
|----------|--------|
| `available()` | `true` on macOS with AppKit; else `false` |
| `open(w, h, title)` | create window; bool |
| `title(s)` `bg(r,g,b)` `color(r,g,b)` `text(x,y,s)` | setup / draw |
| `run()` | block until window closed |

Colors are floats in `0..1`. Text `(x, y)` uses top-left origin.
