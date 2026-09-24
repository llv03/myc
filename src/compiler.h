#ifndef MYC_COMPILER_H
#define MYC_COMPILER_H

#include "ast.h"
#include "chunk.h"
#include "common.h"
#include "value.h"

typedef enum {
    FUNC_USER = 0,
    FUNC_NATIVE_NET_LISTEN,
    FUNC_NATIVE_NET_ACCEPT,
    FUNC_NATIVE_NET_RECV,
    FUNC_NATIVE_NET_SEND,
    FUNC_NATIVE_NET_CLOSE,
    FUNC_NATIVE_FS_READ,
    FUNC_NATIVE_FS_WRITE,
    FUNC_NATIVE_FS_APPEND,
    FUNC_NATIVE_FS_EXISTS,
    FUNC_NATIVE_FS_REMOVE,
    FUNC_NATIVE_MAP_KEYS,
    FUNC_NATIVE_OS_ARGS,
    FUNC_NATIVE_OS_SYSTEM,
    FUNC_NATIVE_OS_CAPTURE,
    FUNC_NATIVE_OS_GETENV,
    FUNC_NATIVE_OS_CWD,
    FUNC_NATIVE_GFX_AVAILABLE,
    FUNC_NATIVE_GFX_OPEN,
    FUNC_NATIVE_GFX_TITLE,
    FUNC_NATIVE_GFX_BG,
    FUNC_NATIVE_GFX_COLOR,
    FUNC_NATIVE_GFX_TEXT,
    FUNC_NATIVE_GFX_RUN
} FuncKind;

typedef struct {
    char name[MAX_NAME];
    char source_path[MAX_PATH]; /* file this function was compiled from */
    int arity;
    Chunk chunk;
    FuncKind kind;
} FuncObj;

typedef struct {
    char name[MAX_NAME];
    char fields[MAX_FIELDS][MAX_NAME];
    int field_count;
} StructDef;

typedef struct CompileResult {
    FuncObj *functions;
    int func_count;
    int main_index;
    StructDef structs[MAX_STRUCTS];
    int struct_count;
    ObjFunction *native_fns[32]; /* GC roots for builtin ObjFunctions */
    int native_fn_count;
    bool had_error;
} CompileResult;

void compile_result_init(CompileResult *r);
void compile_result_free(CompileResult *r);

/* Compile a Program. source_path is used for imports and diagnostics.
 * prefix is prepended to fn names ("" for main, "math." for imported module). */
bool compile_program(Program *prog, CompileResult *out,
                     const char *source_path, const char *prefix);

/* High-level: read, parse, compile a file (handles imports recursively). */
bool compile_file(const char *path, CompileResult *out);

#endif
