#include "compiler.h"
#include "parser.h"
#include "report.h"

typedef struct {
    char name[MAX_NAME];
    int depth;
} Local;

typedef struct {
    FuncObj *func;
    Local locals[MAX_LOCALS];
    int local_count;
    int scope_depth;
    CompileResult *result;
    const char *module_prefix;
    const char *source_path;
    bool had_error;
} Compiler;

static char g_import_stack[MAX_IMPORT_DEPTH][MAX_PATH];
static int  g_import_depth = 0;
static char g_loaded[64][MAX_PATH];
static int  g_loaded_count = 0;

static void c_error(Compiler *c, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    report_errorv(c->source_path, line, 0, REPORT_ERROR, fmt, ap);
    va_end(ap);
    c->had_error = true;
    c->result->had_error = true;
}

static Chunk *cur(Compiler *c) { return &c->func->chunk; }

static void emit_byte(Compiler *c, uint8_t b, int line) {
    chunk_write(cur(c), b, line);
}
static void emit_bytes(Compiler *c, uint8_t a, uint8_t b, int line) {
    emit_byte(c, a, line);
    emit_byte(c, b, line);
}
static void emit_constant(Compiler *c, Value v, int line) {
    int idx = chunk_add_constant(cur(c), v);
    if (idx > 255) { c_error(c, line, "too many constants"); idx = 0; }
    emit_bytes(c, OP_CONSTANT, (uint8_t)idx, line);
}
static void emit_string(Compiler *c, const char *chars, int len, int line) {
    ObjString *s = obj_string_copy(chars, len);
    emit_constant(c, obj_val((Obj *)s), line);
}
static int emit_jump(Compiler *c, uint8_t op, int line) {
    emit_byte(c, op, line);
    emit_byte(c, 0xff, line);
    emit_byte(c, 0xff, line);
    return cur(c)->count - 2;
}
static void patch_jump(Compiler *c, int offset) {
    int jump = cur(c)->count - offset - 2;
    if (jump > 65535) { c_error(c, 0, "jump too large"); return; }
    cur(c)->code[offset]     = (uint8_t)((jump >> 8) & 0xff);
    cur(c)->code[offset + 1] = (uint8_t)(jump & 0xff);
}
static void emit_loop(Compiler *c, int loop_start, int line) {
    emit_byte(c, OP_LOOP, line);
    int jump = cur(c)->count - loop_start + 2;
    if (jump > 65535) { c_error(c, line, "loop body too large"); jump = 0; }
    emit_byte(c, (uint8_t)((jump >> 8) & 0xff), line);
    emit_byte(c, (uint8_t)(jump & 0xff), line);
}

static void begin_scope(Compiler *c) { c->scope_depth++; }
static void end_scope(Compiler *c) {
    c->scope_depth--;
    while (c->local_count > 0 &&
           c->locals[c->local_count - 1].depth > c->scope_depth) {
        emit_byte(c, OP_POP, 0);
        c->local_count--;
    }
}

static int add_local(Compiler *c, const char *name, int line) {
    if (c->local_count >= MAX_LOCALS) {
        c_error(c, line, "too many local variables");
        return 0;
    }
    for (int i = c->local_count - 1; i >= 0; i--) {
        Local *l = &c->locals[i];
        if (l->depth != -1 && l->depth < c->scope_depth) break;
        if (strcmp(l->name, name) == 0) {
            c_error(c, line, "variable '%s' already declared in this scope", name);
            return i;
        }
    }
    Local *l = &c->locals[c->local_count++];
    strncpy(l->name, name, MAX_NAME - 1);
    l->name[MAX_NAME - 1] = '\0';
    l->depth = c->scope_depth;
    return c->local_count - 1;
}

static int resolve_local(Compiler *c, const char *name) {
    for (int i = c->local_count - 1; i >= 0; i--)
        if (strcmp(c->locals[i].name, name) == 0) return i;
    return -1;
}

static int resolve_function(Compiler *c, const char *name) {
    for (int i = 0; i < c->result->func_count; i++)
        if (strcmp(c->result->functions[i].name, name) == 0) return i;
    if (c->module_prefix && c->module_prefix[0]) {
        char q[MAX_NAME * 2];
        snprintf(q, sizeof(q), "%s%s", c->module_prefix, name);
        for (int i = 0; i < c->result->func_count; i++)
            if (strcmp(c->result->functions[i].name, q) == 0) return i;
    }
    return -1;
}

static int resolve_struct(CompileResult *r, const char *name) {
    for (int i = 0; i < r->struct_count; i++)
        if (strcmp(r->structs[i].name, name) == 0) return i;
    return -1;
}

static int make_name_constant(Compiler *c, const char *name, int line) {
    ObjString *s = obj_string_copy(name, (int)strlen(name));
    int idx = chunk_add_constant(cur(c), obj_val((Obj *)s));
    if (idx > 255) { c_error(c, line, "too many constants"); return 0; }
    return idx;
}

static void compile_expr(Compiler *c, Expr *e);
static void compile_stmt(Compiler *c, Stmt *s);

static void compile_expr(Compiler *c, Expr *e) {
    if (!e) { emit_byte(c, OP_NIL, 0); return; }
    switch (e->kind) {
        case EXPR_LITERAL:
            switch (e->as.literal.lit_kind) {
                case LIT_NIL: emit_byte(c, OP_NIL, e->line); break;
                case LIT_BOOL:
                    emit_byte(c, e->as.literal.bool_val ? OP_TRUE : OP_FALSE, e->line);
                    break;
                case LIT_INT:
                    emit_constant(c, int_val(e->as.literal.int_val), e->line);
                    break;
                case LIT_FLOAT:
                    emit_constant(c, float_val(e->as.literal.float_val), e->line);
                    break;
                case LIT_STRING:
                    emit_string(c, e->as.literal.str_val, e->as.literal.str_len, e->line);
                    break;
            }
            break;

        case EXPR_VAR: {
            int slot = resolve_local(c, e->as.var.name);
            if (slot >= 0) {
                emit_bytes(c, OP_GET_LOCAL, (uint8_t)slot, e->line);
                break;
            }
            int fid = resolve_function(c, e->as.var.name);
            if (fid >= 0) {
                emit_bytes(c, OP_CLOSURE, (uint8_t)fid, e->line);
                break;
            }
            c_error(c, e->line, "undefined variable '%s'", e->as.var.name);
            emit_byte(c, OP_NIL, e->line);
            break;
        }

        case EXPR_ASSIGN: {
            compile_expr(c, e->as.assign.value);
            int slot = resolve_local(c, e->as.assign.name);
            if (slot < 0) {
                c_error(c, e->line, "undefined variable '%s'", e->as.assign.name);
                break;
            }
            emit_bytes(c, OP_SET_LOCAL, (uint8_t)slot, e->line);
            break;
        }

        case EXPR_UNARY:
            compile_expr(c, e->as.unary.operand);
            emit_byte(c, e->as.unary.op == TOK_MINUS ? OP_NEGATE : OP_NOT, e->line);
            break;

        case EXPR_BINARY: {
            compile_expr(c, e->as.binary.left);
            compile_expr(c, e->as.binary.right);
            uint8_t op = OP_ADD;
            switch (e->as.binary.op) {
                case TOK_PLUS:    op = OP_ADD; break;
                case TOK_MINUS:   op = OP_SUB; break;
                case TOK_STAR:    op = OP_MUL; break;
                case TOK_SLASH:   op = OP_DIV; break;
                case TOK_PERCENT: op = OP_MOD; break;
                case TOK_EQEQ:    op = OP_EQ;  break;
                case TOK_BANGEQ:  op = OP_NE;  break;
                case TOK_LT:      op = OP_LT;  break;
                case TOK_LTEQ:    op = OP_LE;  break;
                case TOK_GT:      op = OP_GT;  break;
                case TOK_GTEQ:    op = OP_GE;  break;
                default: c_error(c, e->line, "unknown binary operator"); break;
            }
            emit_byte(c, op, e->line);
            break;
        }

        case EXPR_CALL: {
            if (e->as.call.callee->kind == EXPR_VAR && e->as.call.argc == 1) {
                const char *n = e->as.call.callee->as.var.name;
                if (strcmp(n, "len") == 0) {
                    compile_expr(c, e->as.call.args[0]);
                    emit_byte(c, OP_LEN, e->line);
                    break;
                }
                if (strcmp(n, "str") == 0) {
                    compile_expr(c, e->as.call.args[0]);
                    emit_byte(c, OP_STR, e->line);
                    break;
                }
                if (strcmp(n, "type") == 0) {
                    compile_expr(c, e->as.call.args[0]);
                    emit_byte(c, OP_TYPE, e->line);
                    break;
                }
            }
            if (e->as.call.callee->kind == EXPR_GET_PROP &&
                e->as.call.callee->as.get_prop.object->kind == EXPR_VAR) {
                char qname[MAX_NAME * 2];
                snprintf(qname, sizeof(qname), "%s.%s",
                         e->as.call.callee->as.get_prop.object->as.var.name,
                         e->as.call.callee->as.get_prop.name);
                int fid = resolve_function(c, qname);
                if (fid >= 0) {
                    emit_bytes(c, OP_CLOSURE, (uint8_t)fid, e->line);
                    for (int i = 0; i < e->as.call.argc; i++)
                        compile_expr(c, e->as.call.args[i]);
                    emit_bytes(c, OP_CALL, (uint8_t)e->as.call.argc, e->line);
                    break;
                }
            }
            compile_expr(c, e->as.call.callee);
            for (int i = 0; i < e->as.call.argc; i++)
                compile_expr(c, e->as.call.args[i]);
            emit_bytes(c, OP_CALL, (uint8_t)e->as.call.argc, e->line);
            break;
        }

        case EXPR_LIST:
            for (int i = 0; i < e->as.list.count; i++)
                compile_expr(c, e->as.list.items[i]);
            emit_bytes(c, OP_BUILD_LIST, (uint8_t)e->as.list.count, e->line);
            break;

        case EXPR_MAP:
            for (int i = 0; i < e->as.map.count; i++) {
                compile_expr(c, e->as.map.keys[i]);
                compile_expr(c, e->as.map.vals[i]);
            }
            emit_bytes(c, OP_BUILD_MAP, (uint8_t)e->as.map.count, e->line);
            break;

        case EXPR_STRUCT_NEW: {
            int tid = resolve_struct(c->result, e->as.struct_new.type_name);
            if (tid < 0) {
                c_error(c, e->line, "undefined struct type '%s'",
                        e->as.struct_new.type_name);
                emit_byte(c, OP_NIL, e->line);
                break;
            }
            StructDef *def = &c->result->structs[tid];
            for (int fi = 0; fi < def->field_count; fi++) {
                int found = -1;
                for (int j = 0; j < e->as.struct_new.count; j++) {
                    if (strcmp(e->as.struct_new.fields[j], def->fields[fi]) == 0) {
                        found = j; break;
                    }
                }
                if (found >= 0)
                    compile_expr(c, e->as.struct_new.values[found]);
                else
                    emit_byte(c, OP_NIL, e->line);
            }
            emit_bytes(c, OP_BUILD_STRUCT, (uint8_t)tid, e->line);
            break;
        }

        case EXPR_INDEX:
            compile_expr(c, e->as.index.object);
            compile_expr(c, e->as.index.index);
            emit_byte(c, OP_INDEX_GET, e->line);
            break;

        case EXPR_INDEX_ASSIGN:
            compile_expr(c, e->as.index_assign.object);
            compile_expr(c, e->as.index_assign.index);
            compile_expr(c, e->as.index_assign.value);
            emit_byte(c, OP_INDEX_SET, e->line);
            break;

        case EXPR_GET_PROP: {
            if (e->as.get_prop.object->kind == EXPR_VAR) {
                char qname[MAX_NAME * 2];
                snprintf(qname, sizeof(qname), "%s.%s",
                         e->as.get_prop.object->as.var.name,
                         e->as.get_prop.name);
                int fid = resolve_function(c, qname);
                if (fid >= 0) {
                    emit_bytes(c, OP_CLOSURE, (uint8_t)fid, e->line);
                    break;
                }
            }
            compile_expr(c, e->as.get_prop.object);
            int ni = make_name_constant(c, e->as.get_prop.name, e->line);
            emit_bytes(c, OP_GET_PROP, (uint8_t)ni, e->line);
            break;
        }

        case EXPR_SET_PROP: {
            compile_expr(c, e->as.set_prop.object);
            compile_expr(c, e->as.set_prop.value);
            int ni = make_name_constant(c, e->as.set_prop.name, e->line);
            emit_bytes(c, OP_SET_PROP, (uint8_t)ni, e->line);
            break;
        }
    }
}

static void compile_stmt(Compiler *c, Stmt *s) {
    if (!s) return;
    switch (s->kind) {
        case STMT_EXPR:
            compile_expr(c, s->as.expr.expr);
            emit_byte(c, OP_POP, s->line);
            break;
        case STMT_PRINT:
            compile_expr(c, s->as.print.expr);
            emit_byte(c, OP_PRINT, s->line);
            break;
        case STMT_LET: {
            if (s->as.let.init)
                compile_expr(c, s->as.let.init);
            else
                emit_byte(c, OP_NIL, s->line);
            add_local(c, s->as.let.name, s->line);
            break;
        }
        case STMT_BLOCK:
            begin_scope(c);
            for (int i = 0; i < s->as.block.count; i++)
                compile_stmt(c, s->as.block.stmts[i]);
            end_scope(c);
            break;
        case STMT_IF: {
            compile_expr(c, s->as.ifs.cond);
            int then_jump = emit_jump(c, OP_JUMP_IF_FALSE, s->line);
            emit_byte(c, OP_POP, s->line);
            compile_stmt(c, s->as.ifs.then_branch);
            int else_jump = emit_jump(c, OP_JUMP, s->line);
            patch_jump(c, then_jump);
            emit_byte(c, OP_POP, s->line);
            if (s->as.ifs.else_branch)
                compile_stmt(c, s->as.ifs.else_branch);
            patch_jump(c, else_jump);
            break;
        }
        case STMT_WHILE: {
            int loop_start = cur(c)->count;
            compile_expr(c, s->as.whiles.cond);
            int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, s->line);
            emit_byte(c, OP_POP, s->line);
            compile_stmt(c, s->as.whiles.body);
            emit_loop(c, loop_start, s->line);
            patch_jump(c, exit_jump);
            emit_byte(c, OP_POP, s->line);
            break;
        }
        case STMT_FOR: {
            if (s->as.fors.is_for_in) {
                begin_scope(c);
                compile_expr(c, s->as.fors.for_in_iter);
                add_local(c, "__iter", s->line);
                emit_constant(c, int_val(0), s->line);
                add_local(c, "__i", s->line);
                int iter_slot = resolve_local(c, "__iter");
                int i_slot = resolve_local(c, "__i");

                int loop_start = cur(c)->count;
                emit_bytes(c, OP_GET_LOCAL, (uint8_t)i_slot, s->line);
                emit_bytes(c, OP_GET_LOCAL, (uint8_t)iter_slot, s->line);
                emit_byte(c, OP_LEN, s->line);
                emit_byte(c, OP_LT, s->line);
                int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, s->line);
                emit_byte(c, OP_POP, s->line);

                begin_scope(c);
                emit_bytes(c, OP_GET_LOCAL, (uint8_t)iter_slot, s->line);
                emit_bytes(c, OP_GET_LOCAL, (uint8_t)i_slot, s->line);
                emit_byte(c, OP_INDEX_GET, s->line);
                add_local(c, s->as.fors.for_in_name, s->line);
                compile_stmt(c, s->as.fors.body);
                end_scope(c);

                emit_bytes(c, OP_GET_LOCAL, (uint8_t)i_slot, s->line);
                emit_constant(c, int_val(1), s->line);
                emit_byte(c, OP_ADD, s->line);
                emit_bytes(c, OP_SET_LOCAL, (uint8_t)i_slot, s->line);
                emit_byte(c, OP_POP, s->line);

                emit_loop(c, loop_start, s->line);
                patch_jump(c, exit_jump);
                emit_byte(c, OP_POP, s->line);
                end_scope(c);
            } else {
                begin_scope(c);
                if (s->as.fors.init) compile_stmt(c, s->as.fors.init);
                int loop_start = cur(c)->count;
                if (s->as.fors.cond)
                    compile_expr(c, s->as.fors.cond);
                else
                    emit_byte(c, OP_TRUE, s->line);
                int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, s->line);
                emit_byte(c, OP_POP, s->line);
                compile_stmt(c, s->as.fors.body);
                if (s->as.fors.incr) {
                    compile_expr(c, s->as.fors.incr);
                    emit_byte(c, OP_POP, s->line);
                }
                emit_loop(c, loop_start, s->line);
                patch_jump(c, exit_jump);
                emit_byte(c, OP_POP, s->line);
                end_scope(c);
            }
            break;
        }
        case STMT_RETURN:
            if (s->as.ret.value)
                compile_expr(c, s->as.ret.value);
            else
                emit_byte(c, OP_NIL, s->line);
            emit_byte(c, OP_RETURN, s->line);
            break;
        case STMT_FN:
        case STMT_STRUCT:
        case STMT_IMPORT:
            break;
    }
}

static void compiler_init(Compiler *c, FuncObj *func, CompileResult *result,
                          const char *module_prefix, const char *source_path) {
    c->func = func;
    c->local_count = 0;
    c->scope_depth = 0;
    c->result = result;
    c->module_prefix = module_prefix ? module_prefix : "";
    c->source_path = source_path ? source_path : "";
    c->had_error = false;
    Local *l = &c->locals[c->local_count++];
    l->name[0] = '\0';
    l->depth = 0;
}

static FuncObj *alloc_func(CompileResult *r, const char *name, int arity,
                           FuncKind kind) {
    if (r->func_count >= MAX_FUNCS) {
        report_error(NULL, 0, 0, REPORT_ERROR, "too many functions");
        exit(1);
    }
    FuncObj *nf = realloc(r->functions,
                          (size_t)(r->func_count + 1) * sizeof(FuncObj));
    if (!nf) { fprintf(stderr, "oom\n"); exit(1); }
    r->functions = nf;
    FuncObj *f = &r->functions[r->func_count++];
    strncpy(f->name, name, MAX_NAME - 1);
    f->name[MAX_NAME - 1] = '\0';
    f->source_path[0] = '\0';
    f->arity = arity;
    f->kind = kind;
    chunk_init(&f->chunk);
    return f;
}

static void compile_function_body(CompileResult *r, FuncObj *func,
                                  char params[][MAX_NAME], int arity,
                                  Stmt *body, const char *module_prefix,
                                  const char *source_path) {
    Compiler c;
    if (source_path) {
        strncpy(func->source_path, source_path, MAX_PATH - 1);
        func->source_path[MAX_PATH - 1] = '\0';
    }
    compiler_init(&c, func, r, module_prefix, source_path);
    begin_scope(&c);
    for (int i = 0; i < arity; i++)
        add_local(&c, params[i], body ? body->line : 0);
    if (body && body->kind == STMT_BLOCK) {
        for (int i = 0; i < body->as.block.count; i++)
            compile_stmt(&c, body->as.block.stmts[i]);
    } else if (body) {
        compile_stmt(&c, body);
    }
    emit_byte(&c, OP_NIL, 0);
    emit_byte(&c, OP_RETURN, 0);
    end_scope(&c);
}

void compile_result_init(CompileResult *r) {
    r->functions = NULL;
    r->func_count = 0;
    r->main_index = -1;
    r->struct_count = 0;
    r->native_fn_count = 0;
    r->had_error = false;
    memset(r->native_fns, 0, sizeof(r->native_fns));
}

void compile_result_free(CompileResult *r) {
    if (!r) return;
    for (int i = 0; i < r->func_count; i++)
        chunk_free(&r->functions[i].chunk);
    free(r->functions);
    compile_result_init(r);
}

static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static void dirname_of(const char *path, char *out, size_t outsz) {
    const char *slash = NULL;
    for (const char *p = path; *p; p++)
        if (*p == '/') slash = p;
    if (!slash) { snprintf(out, outsz, "."); return; }
    size_t n = (size_t)(slash - path);
    if (n == 0) n = 1;
    if (n >= outsz) n = outsz - 1;
    memcpy(out, path, n);
    out[n] = '\0';
}

static bool file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

/* Entry script path: used to walk up looking for builtins/. */
static char g_entry_path[MAX_PATH];

static bool try_cand(const char *path, char *resolved) {
    if (!file_exists(path)) return false;
    strncpy(resolved, path, MAX_PATH - 1);
    resolved[MAX_PATH - 1] = '\0';
    return true;
}

/* Walk up from start_dir looking for builtins/<spec>.my */
static bool find_builtins_walking(const char *start_dir, const char *spec,
                                  char *resolved) {
    char dir[MAX_PATH];
    strncpy(dir, start_dir, MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';
    for (int depth = 0; depth < 64; depth++) {
        char cand[MAX_PATH];
        snprintf(cand, sizeof(cand), "%s/builtins/%s.my", dir, spec);
        if (try_cand(cand, resolved)) return true;
        if (strcmp(dir, "/") == 0) break;
        if (strcmp(dir, ".") == 0) {
            snprintf(cand, sizeof(cand), "builtins/%s.my", spec);
            return try_cand(cand, resolved);
        }
        char parent[MAX_PATH];
        dirname_of(dir, parent, sizeof(parent));
        if (strcmp(parent, dir) == 0) break;
        strncpy(dir, parent, MAX_PATH - 1);
        dir[MAX_PATH - 1] = '\0';
    }
    return false;
}

static bool resolve_import_path(const char *importer, const char *spec,
                                bool from_string, char *resolved) {
    char dir[MAX_PATH];
    dirname_of(importer, dir, sizeof(dir));
    char cand[MAX_PATH];
    int n = 0;
    char list[24][MAX_PATH];

    if (from_string) {
        if (spec[0] == '/')
            snprintf(list[n++], MAX_PATH, "%s", spec);
        else
            snprintf(list[n++], MAX_PATH, "%s/%s", dir, spec);
    } else {
        /* 1-4: relative to importer */
        snprintf(list[n++], MAX_PATH, "%s/%s.my", dir, spec);
        snprintf(list[n++], MAX_PATH, "%s/lib/%s.my", dir, spec);
        snprintf(list[n++], MAX_PATH, "%s/packages/%s.my", dir, spec);
        snprintf(list[n++], MAX_PATH, "%s/packages/%s/%s.my", dir, spec, spec);
        /* 5: builtins next to importer */
        snprintf(list[n++], MAX_PATH, "%s/builtins/%s.my", dir, spec);
        /* 6-9: relative to cwd */
        snprintf(list[n++], MAX_PATH, "%s.my", spec);
        snprintf(list[n++], MAX_PATH, "lib/%s.my", spec);
        snprintf(list[n++], MAX_PATH, "packages/%s.my", spec);
        snprintf(list[n++], MAX_PATH, "packages/%s/%s.my", spec, spec);
        /* 10: builtins under cwd */
        snprintf(list[n++], MAX_PATH, "builtins/%s.my", spec);
    }
    for (int i = 0; i < n; i++) {
        if (try_cand(list[i], resolved)) return true;
    }
    if (from_string) return false;

    /* Walk up from importer directory looking for builtins/<spec>.my */
    if (find_builtins_walking(dir, spec, resolved)) return true;

    /* Walk up from entry script directory */
    if (g_entry_path[0]) {
        char entry_dir[MAX_PATH];
        dirname_of(g_entry_path, entry_dir, sizeof(entry_dir));
        if (find_builtins_walking(entry_dir, spec, resolved)) return true;
    }

    /* MYC_HOME/builtins/<spec>.my */
    const char *home = getenv("MYC_HOME");
    if (home && home[0]) {
        snprintf(cand, sizeof(cand), "%s/builtins/%s.my", home, spec);
        if (try_cand(cand, resolved)) return true;
    }
    return false;
}

static bool path_in_stack(const char *path) {
    for (int i = 0; i < g_import_depth; i++)
        if (strcmp(g_import_stack[i], path) == 0) return true;
    return false;
}
static bool path_loaded(const char *path) {
    for (int i = 0; i < g_loaded_count; i++)
        if (strcmp(g_loaded[i], path) == 0) return true;
    return false;
}
static void mark_loaded(const char *path) {
    if (g_loaded_count >= 64) return;
    strncpy(g_loaded[g_loaded_count], path, MAX_PATH - 1);
    g_loaded[g_loaded_count][MAX_PATH - 1] = '\0';
    g_loaded_count++;
}

static bool compile_program_inner(Program *prog, CompileResult *out,
                                  const char *source_path, const char *prefix,
                                  bool is_main);

static bool compile_module_file(const char *path, CompileResult *out,
                                const char *prefix) {
    if (g_import_depth >= MAX_IMPORT_DEPTH) {
        report_error(NULL, 0, 0, REPORT_ERROR, "import nesting too deep");
        out->had_error = true;
        return false;
    }
    strncpy(g_import_stack[g_import_depth], path, MAX_PATH - 1);
    g_import_stack[g_import_depth][MAX_PATH - 1] = '\0';
    g_import_depth++;

    char *source = read_file_contents(path);
    if (!source) {
        report_error(path, 0, 0, REPORT_ERROR, "could not read file");
        out->had_error = true;
        g_import_depth--;
        return false;
    }
    Program prog = {0};
    if (!parse(source, path, &prog)) {
        program_free(&prog);
        free(source);
        out->had_error = true;
        g_import_depth--;
        return false;
    }
    bool ok = compile_program_inner(&prog, out, path, prefix, false);
    program_free(&prog);
    free(source);
    if (ok) mark_loaded(path);
    g_import_depth--;
    return ok;
}

static bool handle_imports(Program *prog, CompileResult *out,
                           const char *source_path) {
    for (int i = 0; i < prog->count; i++) {
        Stmt *s = prog->stmts[i];
        if (s->kind != STMT_IMPORT) continue;

        char resolved[MAX_PATH];
        if (!resolve_import_path(source_path, s->as.import.path,
                                 s->as.import.from_string, resolved)) {
            report_error(source_path, s->line, 0, REPORT_ERROR,
                         "cannot resolve import '%s'", s->as.import.path);
            out->had_error = true;
            return false;
        }
        if (path_in_stack(resolved)) {
            report_error(source_path, s->line, 0, REPORT_ERROR,
                         "circular import involving '%s'", resolved);
            out->had_error = true;
            return false;
        }
        if (path_loaded(resolved)) continue;

        char mod_prefix[MAX_NAME + 2];
        snprintf(mod_prefix, sizeof(mod_prefix), "%s.", s->as.import.module);
        if (!compile_module_file(resolved, out, mod_prefix))
            return false;
    }
    return true;
}

static bool compile_program_inner(Program *prog, CompileResult *out,
                                  const char *source_path, const char *prefix,
                                  bool is_main) {
    if (!handle_imports(prog, out, source_path))
        return false;

    for (int i = 0; i < prog->count; i++) {
        Stmt *s = prog->stmts[i];
        if (s->kind != STMT_STRUCT) continue;
        if (out->struct_count >= MAX_STRUCTS) {
            report_error(source_path, s->line, 0, REPORT_ERROR, "too many structs");
            out->had_error = true;
            return false;
        }
        if (resolve_struct(out, s->as.strukt.name) >= 0) {
            report_error(source_path, s->line, 0, REPORT_ERROR,
                         "duplicate struct '%s'", s->as.strukt.name);
            out->had_error = true;
            continue;
        }
        StructDef *d = &out->structs[out->struct_count++];
        strncpy(d->name, s->as.strukt.name, MAX_NAME - 1);
        d->name[MAX_NAME - 1] = '\0';
        d->field_count = s->as.strukt.field_count;
        for (int f = 0; f < d->field_count; f++) {
            strncpy(d->fields[f], s->as.strukt.fields[f], MAX_NAME - 1);
            d->fields[f][MAX_NAME - 1] = '\0';
        }
    }

    int fn_start = out->func_count;
    for (int i = 0; i < prog->count; i++) {
        Stmt *s = prog->stmts[i];
        if (s->kind != STMT_FN) continue;
        char full[MAX_NAME];
        if (prefix && prefix[0])
            snprintf(full, sizeof(full), "%s%s", prefix, s->as.fn.name);
        else
            snprintf(full, sizeof(full), "%s", s->as.fn.name);
        for (int j = 0; j < out->func_count; j++) {
            if (strcmp(out->functions[j].name, full) == 0) {
                report_error(source_path, s->line, 0, REPORT_ERROR,
                             "duplicate function '%s'", full);
                out->had_error = true;
            }
        }
        alloc_func(out, full, s->as.fn.arity, FUNC_USER);
    }

    int fi = fn_start;
    for (int i = 0; i < prog->count; i++) {
        Stmt *s = prog->stmts[i];
        if (s->kind != STMT_FN) continue;
        FuncObj *f = &out->functions[fi++];
        strncpy(f->source_path, source_path, MAX_PATH - 1);
        f->source_path[MAX_PATH - 1] = '\0';
        compile_function_body(out, f, s->as.fn.params, s->as.fn.arity,
                              s->as.fn.body, prefix, source_path);
    }

    if (is_main) {
        FuncObj *mainf = alloc_func(out, "<main>", 0, FUNC_USER);
        out->main_index = out->func_count - 1;
        strncpy(mainf->source_path, source_path, MAX_PATH - 1);
        mainf->source_path[MAX_PATH - 1] = '\0';
        Compiler c;
        compiler_init(&c, mainf, out, "", source_path);
        begin_scope(&c);
        for (int i = 0; i < prog->count; i++) {
            Stmt *s = prog->stmts[i];
            if (s->kind == STMT_FN || s->kind == STMT_STRUCT ||
                s->kind == STMT_IMPORT)
                continue;
            compile_stmt(&c, s);
        }
        emit_byte(&c, OP_NIL, 0);
        emit_byte(&c, OP_RETURN, 0);
        end_scope(&c);
        if (c.had_error) out->had_error = true;
    }
    return !out->had_error;
}

bool compile_program(Program *prog, CompileResult *out,
                     const char *source_path, const char *prefix) {
    return compile_program_inner(prog, out, source_path, prefix, true);
}

static void register_native_builtins(CompileResult *out) {
    alloc_func(out, "__net_listen", 1, FUNC_NATIVE_NET_LISTEN);
    alloc_func(out, "__net_accept", 1, FUNC_NATIVE_NET_ACCEPT);
    alloc_func(out, "__net_recv",   2, FUNC_NATIVE_NET_RECV);
    alloc_func(out, "__net_send",   2, FUNC_NATIVE_NET_SEND);
    alloc_func(out, "__net_close",  1, FUNC_NATIVE_NET_CLOSE);
    alloc_func(out, "__fs_read",    1, FUNC_NATIVE_FS_READ);
    alloc_func(out, "__fs_write",   2, FUNC_NATIVE_FS_WRITE);
    alloc_func(out, "__fs_append",  2, FUNC_NATIVE_FS_APPEND);
    alloc_func(out, "__fs_exists",  1, FUNC_NATIVE_FS_EXISTS);
    alloc_func(out, "__fs_remove",  1, FUNC_NATIVE_FS_REMOVE);
    alloc_func(out, "__map_keys",   1, FUNC_NATIVE_MAP_KEYS);
    alloc_func(out, "__args",       0, FUNC_NATIVE_OS_ARGS);
    alloc_func(out, "__system",     1, FUNC_NATIVE_OS_SYSTEM);
    alloc_func(out, "__capture",    1, FUNC_NATIVE_OS_CAPTURE);
    alloc_func(out, "__getenv",     1, FUNC_NATIVE_OS_GETENV);
    alloc_func(out, "__cwd",        0, FUNC_NATIVE_OS_CWD);
    alloc_func(out, "__gfx_available", 0, FUNC_NATIVE_GFX_AVAILABLE);
    alloc_func(out, "__gfx_open",   3, FUNC_NATIVE_GFX_OPEN);
    alloc_func(out, "__gfx_title",  1, FUNC_NATIVE_GFX_TITLE);
    alloc_func(out, "__gfx_bg",     3, FUNC_NATIVE_GFX_BG);
    alloc_func(out, "__gfx_color",  3, FUNC_NATIVE_GFX_COLOR);
    alloc_func(out, "__gfx_text",   3, FUNC_NATIVE_GFX_TEXT);
    alloc_func(out, "__gfx_run",    0, FUNC_NATIVE_GFX_RUN);
}

bool compile_file(const char *path, CompileResult *out) {
    compile_result_init(out);
    g_import_depth = 0;
    g_loaded_count = 0;
    g_entry_path[0] = '\0';
    strncpy(g_entry_path, path, MAX_PATH - 1);
    g_entry_path[MAX_PATH - 1] = '\0';
    gc_init();
    register_native_builtins(out);

    char *source = read_file_contents(path);
    if (!source) {
        report_error(path, 0, 0, REPORT_ERROR, "could not open file");
        out->had_error = true;
        return false;
    }
    Program prog = {0};
    if (!parse(source, path, &prog)) {
        program_free(&prog);
        free(source);
        out->had_error = true;
        return false;
    }

    strncpy(g_import_stack[g_import_depth], path, MAX_PATH - 1);
    g_import_stack[g_import_depth][MAX_PATH - 1] = '\0';
    g_import_depth++;

    bool ok = compile_program_inner(&prog, out, path, "", true);

    g_import_depth--;
    program_free(&prog);
    free(source);
    if (ok) mark_loaded(path);
    return ok && !out->had_error;
}
