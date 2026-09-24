# myc

Small C11 language with a stack-based bytecode VM. Source files use the `.my`
extension. Pipeline: lex, parse (AST), compile (bytecode), run (call-frame VM).
Values: nil, bool, int, float, string, list, map, struct, function. Control
flow includes `if`/`while`/`for` (aliases `when`/`loop`), compile-time imports,
and a `builtins/` stdlib (math, net/http, fs, os, json, csv, graphics).

## Requirements

- C11 compiler (`cc`)
- POSIX (sockets, `fork`/`pipe` for `os.capture`)
- macOS AppKit only for the graphics package (stub elsewhere)

## Build

```bash
make
```

On Darwin, `make` also builds Objective-C graphics and links AppKit. Elsewhere
the graphics stub is used.

## Run

```bash
./myc examples/fib.my
./myc examples/args_demo.my foo bar
./myc -d examples/hello.my
```

`-d` disassembles bytecode, then runs. Extra args after the script path are
available via `os.args()` / `__args()`.

Exit codes: `0` ok, `65` parse/compile error, `70` runtime error.

## Tests

```bash
make test
```

## Documentation

- Language, stdlib, VM, errors, graphics: [`docs/`](docs/)
- Static site (HTML): [`website/`](website/) - open `website/index.html` in a
  browser, or `cd website && python3 -m http.server`
- Rendered docs: [`website/docs/`](website/docs/) (README, overview, language,
  stdlib, errors, VM, graphics)

## Layout

```
myc/
  src/           C11 VM, lexer, parser, compiler, natives
  builtins/      stdlib packages (.my)
  examples/      sample programs
  tests/         test runner
  docs/          markdown documentation
  website/       static HTML docs
  Makefile
  README.md
```

## License

No license file yet.
