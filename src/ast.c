#include "ast.h"

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

static void copy_name(char *dst, const char *src, int len) {
    if (len >= MAX_NAME) len = MAX_NAME - 1;
    memcpy(dst, src, (size_t)len);
    dst[len] = '\0';
}

static Expr *expr_new(ExprKind kind, int line) {
    Expr *e = xmalloc(sizeof(Expr));
    memset(e, 0, sizeof(Expr));
    e->kind = kind;
    e->line = line;
    return e;
}

Expr *expr_lit_nil(int line) {
    Expr *e = expr_new(EXPR_LITERAL, line);
    e->as.literal.lit_kind = LIT_NIL;
    return e;
}

Expr *expr_lit_bool(bool b, int line) {
    Expr *e = expr_new(EXPR_LITERAL, line);
    e->as.literal.lit_kind = LIT_BOOL;
    e->as.literal.bool_val = b;
    return e;
}

Expr *expr_lit_int(int64_t v, int line) {
    Expr *e = expr_new(EXPR_LITERAL, line);
    e->as.literal.lit_kind = LIT_INT;
    e->as.literal.int_val = v;
    return e;
}

Expr *expr_lit_float(double v, int line) {
    Expr *e = expr_new(EXPR_LITERAL, line);
    e->as.literal.lit_kind = LIT_FLOAT;
    e->as.literal.float_val = v;
    return e;
}

Expr *expr_lit_string(const char *chars, int len, int line) {
    Expr *e = expr_new(EXPR_LITERAL, line);
    e->as.literal.lit_kind = LIT_STRING;
    e->as.literal.str_val = xmalloc((size_t)len + 1);
    memcpy(e->as.literal.str_val, chars, (size_t)len);
    e->as.literal.str_val[len] = '\0';
    e->as.literal.str_len = len;
    return e;
}

Expr *expr_var(const char *name, int len, int line) {
    Expr *e = expr_new(EXPR_VAR, line);
    copy_name(e->as.var.name, name, len);
    return e;
}

Expr *expr_binary(TokenType op, Expr *l, Expr *r, int line) {
    Expr *e = expr_new(EXPR_BINARY, line);
    e->as.binary.op = op;
    e->as.binary.left = l;
    e->as.binary.right = r;
    return e;
}

Expr *expr_unary(TokenType op, Expr *operand, int line) {
    Expr *e = expr_new(EXPR_UNARY, line);
    e->as.unary.op = op;
    e->as.unary.operand = operand;
    return e;
}

Expr *expr_assign(const char *name, int len, Expr *value, int line) {
    Expr *e = expr_new(EXPR_ASSIGN, line);
    copy_name(e->as.assign.name, name, len);
    e->as.assign.value = value;
    return e;
}

Expr *expr_call(Expr *callee, Expr **args, int argc, int line) {
    Expr *e = expr_new(EXPR_CALL, line);
    e->as.call.callee = callee;
    e->as.call.args = args;
    e->as.call.argc = argc;
    return e;
}

Expr *expr_list(Expr **items, int count, int line) {
    Expr *e = expr_new(EXPR_LIST, line);
    e->as.list.items = items;
    e->as.list.count = count;
    return e;
}

Expr *expr_map(Expr **keys, Expr **vals, int count, int line) {
    Expr *e = expr_new(EXPR_MAP, line);
    e->as.map.keys = keys;
    e->as.map.vals = vals;
    e->as.map.count = count;
    return e;
}

Expr *expr_struct_new(const char *type, int tlen,
                      char fields[][MAX_NAME], Expr **values, int count, int line) {
    Expr *e = expr_new(EXPR_STRUCT_NEW, line);
    copy_name(e->as.struct_new.type_name, type, tlen);
    e->as.struct_new.values = values;
    e->as.struct_new.count = count;
    for (int i = 0; i < count; i++) {
        strncpy(e->as.struct_new.fields[i], fields[i], MAX_NAME - 1);
        e->as.struct_new.fields[i][MAX_NAME - 1] = '\0';
    }
    return e;
}

Expr *expr_index(Expr *object, Expr *index, int line) {
    Expr *e = expr_new(EXPR_INDEX, line);
    e->as.index.object = object;
    e->as.index.index = index;
    return e;
}

Expr *expr_index_assign(Expr *object, Expr *index, Expr *value, int line) {
    Expr *e = expr_new(EXPR_INDEX_ASSIGN, line);
    e->as.index_assign.object = object;
    e->as.index_assign.index = index;
    e->as.index_assign.value = value;
    return e;
}

Expr *expr_get_prop(Expr *object, const char *name, int len, int line) {
    Expr *e = expr_new(EXPR_GET_PROP, line);
    e->as.get_prop.object = object;
    copy_name(e->as.get_prop.name, name, len);
    return e;
}

Expr *expr_set_prop(Expr *object, const char *name, int len, Expr *value, int line) {
    Expr *e = expr_new(EXPR_SET_PROP, line);
    e->as.set_prop.object = object;
    copy_name(e->as.set_prop.name, name, len);
    e->as.set_prop.value = value;
    return e;
}

static Stmt *stmt_new(StmtKind kind, int line) {
    Stmt *s = xmalloc(sizeof(Stmt));
    memset(s, 0, sizeof(Stmt));
    s->kind = kind;
    s->line = line;
    return s;
}

Stmt *stmt_expr(Expr *ex, int line) {
    Stmt *s = stmt_new(STMT_EXPR, line);
    s->as.expr.expr = ex;
    return s;
}

Stmt *stmt_let(const char *name, int len, Expr *init, int line) {
    Stmt *s = stmt_new(STMT_LET, line);
    copy_name(s->as.let.name, name, len);
    s->as.let.init = init;
    return s;
}

Stmt *stmt_block(Stmt **stmts, int count, int line) {
    Stmt *s = stmt_new(STMT_BLOCK, line);
    s->as.block.stmts = stmts;
    s->as.block.count = count;
    return s;
}

Stmt *stmt_if(Expr *cond, Stmt *then_b, Stmt *else_b, int line) {
    Stmt *s = stmt_new(STMT_IF, line);
    s->as.ifs.cond = cond;
    s->as.ifs.then_branch = then_b;
    s->as.ifs.else_branch = else_b;
    return s;
}

Stmt *stmt_while(Expr *cond, Stmt *body, int line) {
    Stmt *s = stmt_new(STMT_WHILE, line);
    s->as.whiles.cond = cond;
    s->as.whiles.body = body;
    return s;
}

Stmt *stmt_for(Stmt *init, Expr *cond, Expr *incr, Stmt *body, int line) {
    Stmt *s = stmt_new(STMT_FOR, line);
    s->as.fors.init = init;
    s->as.fors.cond = cond;
    s->as.fors.incr = incr;
    s->as.fors.body = body;
    s->as.fors.is_for_in = false;
    return s;
}

Stmt *stmt_for_in(const char *name, int len, Expr *iter, Stmt *body, int line) {
    Stmt *s = stmt_new(STMT_FOR, line);
    s->as.fors.is_for_in = true;
    copy_name(s->as.fors.for_in_name, name, len);
    s->as.fors.for_in_iter = iter;
    s->as.fors.body = body;
    return s;
}

Stmt *stmt_return(Expr *value, int line) {
    Stmt *s = stmt_new(STMT_RETURN, line);
    s->as.ret.value = value;
    return s;
}

Stmt *stmt_print(Expr *expr, int line) {
    Stmt *s = stmt_new(STMT_PRINT, line);
    s->as.print.expr = expr;
    return s;
}

Stmt *stmt_fn(const char *name, int len, char params[][MAX_NAME], int arity,
              Stmt *body, int line) {
    Stmt *s = stmt_new(STMT_FN, line);
    copy_name(s->as.fn.name, name, len);
    s->as.fn.arity = arity;
    for (int i = 0; i < arity; i++) {
        strncpy(s->as.fn.params[i], params[i], MAX_NAME - 1);
        s->as.fn.params[i][MAX_NAME - 1] = '\0';
    }
    s->as.fn.body = body;
    return s;
}

Stmt *stmt_struct(const char *name, int len, char fields[][MAX_NAME],
                  int field_count, int line) {
    Stmt *s = stmt_new(STMT_STRUCT, line);
    copy_name(s->as.strukt.name, name, len);
    s->as.strukt.field_count = field_count;
    for (int i = 0; i < field_count; i++) {
        strncpy(s->as.strukt.fields[i], fields[i], MAX_NAME - 1);
        s->as.strukt.fields[i][MAX_NAME - 1] = '\0';
    }
    return s;
}

Stmt *stmt_import(const char *module, int mlen, const char *path, int plen,
                  bool from_string, int line) {
    Stmt *s = stmt_new(STMT_IMPORT, line);
    copy_name(s->as.import.module, module, mlen);
    if (plen >= MAX_PATH) plen = MAX_PATH - 1;
    memcpy(s->as.import.path, path, (size_t)plen);
    s->as.import.path[plen] = '\0';
    s->as.import.from_string = from_string;
    return s;
}

void expr_free(Expr *e) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_LITERAL:
            free(e->as.literal.str_val);
            break;
        case EXPR_BINARY:
            expr_free(e->as.binary.left);
            expr_free(e->as.binary.right);
            break;
        case EXPR_UNARY:
            expr_free(e->as.unary.operand);
            break;
        case EXPR_ASSIGN:
            expr_free(e->as.assign.value);
            break;
        case EXPR_CALL:
            expr_free(e->as.call.callee);
            for (int i = 0; i < e->as.call.argc; i++)
                expr_free(e->as.call.args[i]);
            free(e->as.call.args);
            break;
        case EXPR_LIST:
            for (int i = 0; i < e->as.list.count; i++)
                expr_free(e->as.list.items[i]);
            free(e->as.list.items);
            break;
        case EXPR_MAP:
            for (int i = 0; i < e->as.map.count; i++) {
                expr_free(e->as.map.keys[i]);
                expr_free(e->as.map.vals[i]);
            }
            free(e->as.map.keys);
            free(e->as.map.vals);
            break;
        case EXPR_STRUCT_NEW:
            for (int i = 0; i < e->as.struct_new.count; i++)
                expr_free(e->as.struct_new.values[i]);
            free(e->as.struct_new.values);
            break;
        case EXPR_INDEX:
            expr_free(e->as.index.object);
            expr_free(e->as.index.index);
            break;
        case EXPR_INDEX_ASSIGN:
            expr_free(e->as.index_assign.object);
            expr_free(e->as.index_assign.index);
            expr_free(e->as.index_assign.value);
            break;
        case EXPR_GET_PROP:
            expr_free(e->as.get_prop.object);
            break;
        case EXPR_SET_PROP:
            expr_free(e->as.set_prop.object);
            expr_free(e->as.set_prop.value);
            break;
        default:
            break;
    }
    free(e);
}

void stmt_free(Stmt *s) {
    if (!s) return;
    switch (s->kind) {
        case STMT_EXPR:
            expr_free(s->as.expr.expr);
            break;
        case STMT_LET:
            expr_free(s->as.let.init);
            break;
        case STMT_BLOCK:
            for (int i = 0; i < s->as.block.count; i++)
                stmt_free(s->as.block.stmts[i]);
            free(s->as.block.stmts);
            break;
        case STMT_IF:
            expr_free(s->as.ifs.cond);
            stmt_free(s->as.ifs.then_branch);
            stmt_free(s->as.ifs.else_branch);
            break;
        case STMT_WHILE:
            expr_free(s->as.whiles.cond);
            stmt_free(s->as.whiles.body);
            break;
        case STMT_FOR:
            stmt_free(s->as.fors.init);
            expr_free(s->as.fors.cond);
            expr_free(s->as.fors.incr);
            expr_free(s->as.fors.for_in_iter);
            stmt_free(s->as.fors.body);
            break;
        case STMT_RETURN:
            expr_free(s->as.ret.value);
            break;
        case STMT_PRINT:
            expr_free(s->as.print.expr);
            break;
        case STMT_FN:
            stmt_free(s->as.fn.body);
            break;
        case STMT_STRUCT:
        case STMT_IMPORT:
            break;
    }
    free(s);
}

void program_free(Program *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->count; i++)
        stmt_free(prog->stmts[i]);
    free(prog->stmts);
    prog->stmts = NULL;
    prog->count = 0;
}
