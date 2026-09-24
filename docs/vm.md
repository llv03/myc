# VM

## Pipeline

```
  .my source
      |
      v
  Lexer -> Parser -> Compiler -> VM -> stdout
  tokens    AST      bytecode   stack + frames
```

1. **Lexer** - tokens from source
2. **Parser** - AST
3. **Compiler** - bytecode chunk(s), resolve imports, link functions
4. **VM** - stack machine with call frames

Imports resolve and link at compile time (no runtime `OP_IMPORT`).

Top-level statements compile into an implicit `<main>`. Named `fn`s live in a
function table. `OP_CLOSURE` pushes a callable; `OP_CALL` builds a call frame
or dispatches a C native.

Values are a tagged union. Heap objects (string, list, map, struct, function)
are GC'd.

Bytecode style is clox-inspired; the C is original and kept small.

## GC

Mark-sweep over heap objects. Chosen over refcount for cycle safety. See
`src/value.c` (`gc_collect`, mark bit on objects).

Roots include the VM stack, call frames, and globals held by the VM.

## Bytecode overview

Opcodes (from `src/chunk.h`):

| Opcode | Role |
|--------|------|
| `OP_CONSTANT` `OP_NIL` `OP_TRUE` `OP_FALSE` | literals |
| `OP_ADD` `OP_SUB` `OP_MUL` `OP_DIV` `OP_MOD` | arithmetic |
| `OP_NEGATE` `OP_NOT` | unary |
| `OP_EQ` `OP_NE` `OP_LT` `OP_LE` `OP_GT` `OP_GE` | compare |
| `OP_PRINT` `OP_POP` | print / discard |
| `OP_GET_LOCAL` `OP_SET_LOCAL` | locals |
| `OP_JUMP` `OP_JUMP_IF_FALSE` `OP_LOOP` | control flow |
| `OP_CALL` `OP_RETURN` `OP_CLOSURE` | calls |
| `OP_BUILD_LIST` `OP_BUILD_MAP` `OP_BUILD_STRUCT` | aggregates |
| `OP_INDEX_GET` `OP_INDEX_SET` | index |
| `OP_GET_PROP` `OP_SET_PROP` | fields / qualified names |
| `OP_LEN` `OP_STR` `OP_TYPE` | builtins |

Disassemble with `./myc -d file.my`.

## Out of scope

Classes/inheritance/methods, closures that capture locals, GC tuning,
async/concurrency, a REPL, TLS, HTTP/2, non-blocking sockets.
