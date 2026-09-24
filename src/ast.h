#ifndef MYC_AST_H
#define MYC_AST_H

#include "common.h"
#include "lexer.h"

typedef enum {
    LIT_NIL,
    LIT_BOOL,
    LIT_INT,
    LIT_FLOAT,
    LIT_STRING
} LiteralKind;

typedef enum {
    EXPR_LITERAL,
    EXPR_VAR,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_LIST,
    EXPR_MAP,
    EXPR_STRUCT_NEW,
    EXPR_INDEX,
    EXPR_INDEX_ASSIGN,
    EXPR_GET_PROP,
    EXPR_SET_PROP
} ExprKind;

typedef enum {
    STMT_EXPR,
    STMT_LET,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE,
    STMT_FOR,
    STMT_RETURN,
    STMT_PRINT,
    STMT_FN,
    STMT_STRUCT,
    STMT_IMPORT
} StmtKind;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

struct Expr {
    ExprKind kind;
    int line;
    union {
        struct {
            LiteralKind lit_kind;
            int64_t int_val;
            double float_val;
            bool bool_val;
            char *str_val;   /* owned, for LIT_STRING */
            int str_len;
        } literal;
        struct { char name[MAX_NAME]; } var;
        struct {
            TokenType op;
            Expr *left;
            Expr *right;
        } binary;
        struct {
            TokenType op;
            Expr *operand;
        } unary;
        struct {
            char name[MAX_NAME];
            Expr *value;
        } assign;
        struct {
            Expr *callee;
            Expr **args;
            int argc;
        } call;
        struct {
            Expr **items;
            int count;
        } list;
        struct {
            Expr **keys;
            Expr **vals;
            int count;
        } map;
        struct {
            char type_name[MAX_NAME];
            char fields[MAX_FIELDS][MAX_NAME];
            Expr **values;
            int count;
        } struct_new;
        struct {
            Expr *object;
            Expr *index;
        } index;
        struct {
            Expr *object;
            Expr *index;
            Expr *value;
        } index_assign;
        struct {
            Expr *object;
            char name[MAX_NAME];
        } get_prop;
        struct {
            Expr *object;
            char name[MAX_NAME];
            Expr *value;
        } set_prop;
    } as;
};

struct Stmt {
    StmtKind kind;
    int line;
    union {
        struct { Expr *expr; } expr;
        struct {
            char name[MAX_NAME];
            Expr *init; /* nullable → nil */
        } let;
        struct {
            Stmt **stmts;
            int count;
        } block;
        struct {
            Expr *cond;
            Stmt *then_branch;
            Stmt *else_branch;
        } ifs;
        struct {
            Expr *cond;
            Stmt *body;
        } whiles;
        struct {
            /* C-style: for (init; cond; incr) body
             * For-in:   for (ident in expr) body  (init=NULL, for_in_name set) */
            Stmt *init;          /* let or expr stmt; nullable */
            Expr *cond;          /* nullable → true */
            Expr *incr;          /* nullable */
            Stmt *body;
            bool is_for_in;
            char for_in_name[MAX_NAME];
            Expr *for_in_iter;   /* list/map to iterate */
        } fors;
        struct { Expr *value; } ret;
        struct { Expr *expr; } print;
        struct {
            char name[MAX_NAME];
            char params[MAX_PARAMS][MAX_NAME];
            int arity;
            Stmt *body;
        } fn;
        struct {
            char name[MAX_NAME];
            char fields[MAX_FIELDS][MAX_NAME];
            int field_count;
        } strukt;
        struct {
            char module[MAX_NAME];  /* qualification prefix */
            char path[MAX_PATH];    /* resolved-ish path string from source */
            bool from_string;       /* import "..." vs import name */
        } import;
    } as;
};

typedef struct {
    Stmt **stmts;
    int count;
} Program;

Expr *expr_lit_nil(int line);
Expr *expr_lit_bool(bool b, int line);
Expr *expr_lit_int(int64_t v, int line);
Expr *expr_lit_float(double v, int line);
Expr *expr_lit_string(const char *chars, int len, int line);
Expr *expr_var(const char *name, int len, int line);
Expr *expr_binary(TokenType op, Expr *l, Expr *r, int line);
Expr *expr_unary(TokenType op, Expr *operand, int line);
Expr *expr_assign(const char *name, int len, Expr *value, int line);
Expr *expr_call(Expr *callee, Expr **args, int argc, int line);
Expr *expr_list(Expr **items, int count, int line);
Expr *expr_map(Expr **keys, Expr **vals, int count, int line);
Expr *expr_struct_new(const char *type, int tlen,
                      char fields[][MAX_NAME], Expr **values, int count, int line);
Expr *expr_index(Expr *object, Expr *index, int line);
Expr *expr_index_assign(Expr *object, Expr *index, Expr *value, int line);
Expr *expr_get_prop(Expr *object, const char *name, int len, int line);
Expr *expr_set_prop(Expr *object, const char *name, int len, Expr *value, int line);

Stmt *stmt_expr(Expr *e, int line);
Stmt *stmt_let(const char *name, int len, Expr *init, int line);
Stmt *stmt_block(Stmt **stmts, int count, int line);
Stmt *stmt_if(Expr *cond, Stmt *then_b, Stmt *else_b, int line);
Stmt *stmt_while(Expr *cond, Stmt *body, int line);
Stmt *stmt_for(Stmt *init, Expr *cond, Expr *incr, Stmt *body, int line);
Stmt *stmt_for_in(const char *name, int len, Expr *iter, Stmt *body, int line);
Stmt *stmt_return(Expr *value, int line);
Stmt *stmt_print(Expr *expr, int line);
Stmt *stmt_fn(const char *name, int len, char params[][MAX_NAME], int arity,
              Stmt *body, int line);
Stmt *stmt_struct(const char *name, int len, char fields[][MAX_NAME],
                  int field_count, int line);
Stmt *stmt_import(const char *module, int mlen, const char *path, int plen,
                  bool from_string, int line);

void program_free(Program *prog);
void stmt_free(Stmt *s);
void expr_free(Expr *e);

#endif
