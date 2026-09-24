# Errors

## Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `65` | Parse or compile error |
| `70` | Runtime error |
| `1` | Bad CLI usage (unknown flag, missing file) |

## Format

Errors go to stderr.

Parse/compile:

```
myc: path/to/file.my:12:5: error: unexpected token '}' (expected expression)
```

Runtime:

```
myc: path/to/file.my:3: runtime error: division by zero
  in <main>()
```

## Examples

Intentional failures live under `examples/errors/`:

| File | Kind |
|------|------|
| `examples/errors/bad_parse.my` | parse error |
| `examples/errors/undef_var.my` | compile / name error |
| `examples/errors/div_zero.my` | runtime division by zero |

```bash
./myc examples/errors/div_zero.my
echo $?   # 70
```
