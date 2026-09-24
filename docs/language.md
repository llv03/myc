# Language

Source files use the `.my` extension. Comments are `//` to end of line.
Semicolons are required after statements.

## Grammar sketch

```
program     := decl*
decl        := import_decl | struct_decl | fn_decl | let_decl | stmt
import_decl := "import" (IDENT | STRING) ";"
struct_decl := "struct" IDENT "{" (IDENT [";"|","])* "}" [";"]
fn_decl     := "fn" IDENT "(" params? ")" block
let_decl    := "let" IDENT ("=" expr)? ";"
stmt        := out_stmt | ret_stmt | if_stmt | while_stmt | for_stmt
             | block | expr ";"
if_stmt     := ("if"|"when") "(" expr ")" stmt ("else" stmt)?
while_stmt  := ("while"|"loop") "(" expr ")" stmt
for_stmt    := "for" "(" let_decl_or_expr? ";" expr? ";" expr? ")" stmt
             | "for" "(" IDENT "in" expr ")" stmt
out_stmt    := "out" expr ";"
ret_stmt    := "ret" expr? ";"
block       := "{" decl* "}"

expr        := assign
assign      := equality ("=" assign)?
call        := primary ( "(" args? ")" | "[" expr "]" | "." IDENT )*
primary     := NUMBER | STRING | "true" | "false" | "nil" | IDENT
             | IDENT "{" fields "}"
             | "[" exprs "]"
             | "{" (expr ":" expr)* "}"
             | "(" expr ")"
```

Assignment also covers index and property targets (`a[i] = x`, `p.x = 1`).

## Keywords

| Keep | C-like aliases | New |
|------|----------------|-----|
| `fn` `let` `when` `loop` `ret` `out` `true` `false` | `if`=`when`, `while`=`loop`, `else` | `for` `in` `nil` `struct` `import` |

## Types and truthiness

| Type | Examples |
|------|----------|
| nil | `nil` |
| bool | `true` `false` |
| int | `42` (int64) |
| float | `3.14` (double) |
| string | `"hello"` (heap) |
| list | `[1, 2, 3]` |
| map | `{"a": 1, 2: "x"}` (keys: string or int) |
| struct | `Point { x: 1, y: 2 }` |
| function | first-class via name / qualified import |

Truthiness: `nil`, `false`, `0`, `0.0`, `""`, `[]`, and `{}` are falsey;
everything else is truthy.

Lists grow with append-by-index: `a[len(a)] = x`.

## Operators

| Kind | Ops |
|------|-----|
| Arithmetic | `+ - * / %` |
| Comparison | `== != < <= > >=` |
| Unary | `- !` |

`int`/`int` arithmetic stays int. Mixing with float yields float.
`+` also concatenates strings (the other operand is stringified).

## Control flow

```
if (cond) { ... } else { ... }
when (cond) { ... }

while (cond) { ... }
loop (cond) { ... }

for (let i = 0; i < 10; i = i + 1) { ... }
for (x in xs) { ... }
```

`out expr;` prints. `ret expr;` or `ret;` returns from a function.

## Structs

```
struct Point {
  x;
  y;
}

let p = Point { x: 1, y: 2 };
out(p.x);
p.y = 3;
```

Field list items may be separated by `;` or `,`.

## Imports (compile-time linking)

```
import util;           // -> util.double, util.greet
import "lib/util.my";  // prefix = basename without .my
import math;           // -> math.sqrt from builtins/math.my
```

Resolution order (first hit wins):

1. `<importer_dir>/<name>.my`
2. `<importer_dir>/lib/<name>.my`
3. `<importer_dir>/packages/<name>.my`
4. `<importer_dir>/packages/<name>/<name>.my`
5. `<importer_dir>/builtins/<name>.my`
6. Same patterns from the process cwd (`./`, `lib/`, `packages/`, `builtins/`)
7. Walk up from the importer directory looking for `builtins/<name>.my`
8. Walk up from the entry `.my` file directory looking for `builtins/<name>.my`
9. `$MYC_HOME/builtins/<name>.my` if `MYC_HOME` is set

String imports (`import "path.my"`) resolve relative to the importer (or absolute).

Imported top-level `fn`s become `module.fn`. Structs merge into the global
struct table by name. Top-level statements in imported files are not executed.
Circular imports fail at compile time. There is no runtime `OP_IMPORT`.

## Example

```
fn fib(n) {
  when (n < 2) { ret n; }
  ret fib(n - 1) + fib(n - 2);
}

out(fib(10));
```
