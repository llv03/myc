#include "parser.h"
#include "report.h"

static void error_at(Parser *p, Token *t, const char *msg) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    int expectish = (strncmp(msg, "expected", 8) == 0);
    if (t->type == TOK_ERROR) {
        report_error(p->path, t->line, t->column, REPORT_ERROR, "%s", msg);
    } else if (expectish && t->type == TOK_EOF) {
        report_error(p->path, t->line, t->column, REPORT_ERROR,
                     "unexpected end of file (%s)", msg);
    } else if (expectish) {
        report_error(p->path, t->line, t->column, REPORT_ERROR,
                     "unexpected token '%.*s' (%s)",
                     t->length, t->start, msg);
    } else {
        report_error(p->path, t->line, t->column, REPORT_ERROR, "%s", msg);
    }
    p->had_error = true;
}

static void error(Parser *p, const char *msg) {
    error_at(p, &p->previous, msg);
}

static void error_current(Parser *p, const char *msg) {
    error_at(p, &p->current, msg);
}

static void advance(Parser *p) {
    p->previous = p->current;
    for (;;) {
        p->current = lexer_next(&p->lexer);
        if (p->current.type != TOK_ERROR) break;
        error_current(p, p->current.start);
    }
}

static bool check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static bool match(Parser *p, TokenType type) {
    if (!check(p, type)) return false;
    advance(p);
    return true;
}

static void consume(Parser *p, TokenType type, const char *msg) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error_current(p, msg);
}

static void synchronize(Parser *p) {
    p->panic_mode = false;
    while (p->current.type != TOK_EOF) {
        if (p->previous.type == TOK_SEMI) return;
        switch (p->current.type) {
            case TOK_FN: case TOK_LET: case TOK_WHEN: case TOK_IF:
            case TOK_LOOP: case TOK_WHILE: case TOK_FOR:
            case TOK_RET: case TOK_OUT: case TOK_STRUCT: case TOK_IMPORT:
                return;
            default: break;
        }
        advance(p);
    }
}

static Expr *expression(Parser *p);
static Stmt *declaration(Parser *p);
static Stmt *statement(Parser *p);
static Stmt *block_stmt(Parser *p);
static Stmt *let_declaration(Parser *p);

/* Unescape a string literal token (including quotes). */
static char *unescape_string(const char *start, int length, int *out_len) {
    /* start points at opening ", length includes both quotes */
    const char *src = start + 1;
    int n = length - 2;
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fprintf(stderr, "oom\n"); exit(1); }
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (src[i] == '\\' && i + 1 < n) {
            i++;
            switch (src[i]) {
                case 'n':  buf[j++] = '\n'; break;
                case 't':  buf[j++] = '\t'; break;
                case 'r':  buf[j++] = '\r'; break;
                case '\\': buf[j++] = '\\'; break;
                case '"':  buf[j++] = '"'; break;
                default:   buf[j++] = src[i]; break;
            }
        } else {
            buf[j++] = src[i];
        }
    }
    buf[j] = '\0';
    *out_len = j;
    return buf;
}

static Expr *parse_list(Parser *p) {
    int line = p->previous.line;
    Expr **items = NULL;
    int count = 0, cap = 0;
    if (!check(p, TOK_RBRACKET)) {
        do {
            if (count + 1 > cap) {
                cap = cap < 4 ? 4 : cap * 2;
                items = realloc(items, (size_t)cap * sizeof(Expr *));
                if (!items) { fprintf(stderr, "oom\n"); exit(1); }
            }
            items[count++] = expression(p);
        } while (match(p, TOK_COMMA));
    }
    consume(p, TOK_RBRACKET, "expected ']' after list");
    return expr_list(items, count, line);
}

static Expr *parse_map(Parser *p) {
    int line = p->previous.line;
    Expr **keys = NULL, **vals = NULL;
    int count = 0, cap = 0;
    if (!check(p, TOK_RBRACE)) {
        do {
            if (count + 1 > cap) {
                cap = cap < 4 ? 4 : cap * 2;
                keys = realloc(keys, (size_t)cap * sizeof(Expr *));
                vals = realloc(vals, (size_t)cap * sizeof(Expr *));
                if (!keys || !vals) { fprintf(stderr, "oom\n"); exit(1); }
            }
            keys[count] = expression(p);
            consume(p, TOK_COLON, "expected ':' after map key");
            vals[count] = expression(p);
            count++;
        } while (match(p, TOK_COMMA));
    }
    consume(p, TOK_RBRACE, "expected '}' after map");
    return expr_map(keys, vals, count, line);
}

static Expr *parse_struct_new(Parser *p, Token type_name) {
    /* '{' already matched */
    int line = type_name.line;
    char fields[MAX_FIELDS][MAX_NAME];
    Expr **values = NULL;
    int count = 0, cap = 0;
    if (!check(p, TOK_RBRACE)) {
        do {
            if (count >= MAX_FIELDS) {
                error(p, "too many struct fields");
                break;
            }
            consume(p, TOK_IDENT, "expected field name");
            int len = p->previous.length;
            if (len >= MAX_NAME) len = MAX_NAME - 1;
            memcpy(fields[count], p->previous.start, (size_t)len);
            fields[count][len] = '\0';
            consume(p, TOK_COLON, "expected ':' after field name");
            if (count + 1 > cap) {
                cap = cap < 4 ? 4 : cap * 2;
                values = realloc(values, (size_t)cap * sizeof(Expr *));
                if (!values) { fprintf(stderr, "oom\n"); exit(1); }
            }
            values[count] = expression(p);
            count++;
        } while (match(p, TOK_COMMA));
    }
    consume(p, TOK_RBRACE, "expected '}' after struct literal");
    return expr_struct_new(type_name.start, type_name.length,
                           fields, values, count, line);
}

static Expr *primary(Parser *p) {
    if (match(p, TOK_NIL))   return expr_lit_nil(p->previous.line);
    if (match(p, TOK_TRUE))  return expr_lit_bool(true, p->previous.line);
    if (match(p, TOK_FALSE)) return expr_lit_bool(false, p->previous.line);

    if (match(p, TOK_NUMBER)) {
        char buf[64];
        int len = p->previous.length;
        if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
        memcpy(buf, p->previous.start, (size_t)len);
        buf[len] = '\0';
        bool is_float = false;
        for (int i = 0; i < len; i++) if (buf[i] == '.') { is_float = true; break; }
        if (is_float)
            return expr_lit_float(strtod(buf, NULL), p->previous.line);
        return expr_lit_int((int64_t)strtoll(buf, NULL, 10), p->previous.line);
    }

    if (match(p, TOK_STRING)) {
        int out_len = 0;
        char *chars = unescape_string(p->previous.start, p->previous.length, &out_len);
        Expr *e = expr_lit_string(chars, out_len, p->previous.line);
        free(chars);
        return e;
    }

    if (match(p, TOK_IDENT)) {
        Token name = p->previous;
        /* Ident { ... } is a struct literal in expression context. */
        if (check(p, TOK_LBRACE)) {
            advance(p); /* '{' */
            return parse_struct_new(p, name);
        }
        return expr_var(name.start, name.length, name.line);
    }

    if (match(p, TOK_LBRACKET)) {
        return parse_list(p);
    }

    if (match(p, TOK_LBRACE)) {
        return parse_map(p);
    }

    if (match(p, TOK_LPAREN)) {
        Expr *e = expression(p);
        consume(p, TOK_RPAREN, "expected ')' after expression");
        return e;
    }

    error_current(p, "expected expression");
    return expr_lit_nil(p->current.line);
}

static Expr *finish_call(Parser *p, Expr *callee) {
    Expr **args = NULL;
    int argc = 0, cap = 0;
    if (!check(p, TOK_RPAREN)) {
        do {
            if (argc >= MAX_PARAMS) {
                error(p, "too many arguments");
                break;
            }
            if (argc + 1 > cap) {
                cap = cap < 4 ? 4 : cap * 2;
                args = realloc(args, (size_t)cap * sizeof(Expr *));
                if (!args) { fprintf(stderr, "oom\n"); exit(1); }
            }
            args[argc++] = expression(p);
        } while (match(p, TOK_COMMA));
    }
    consume(p, TOK_RPAREN, "expected ')' after arguments");
    return expr_call(callee, args, argc, p->previous.line);
}

static Expr *call(Parser *p) {
    Expr *e = primary(p);
    for (;;) {
        if (match(p, TOK_LPAREN)) {
            e = finish_call(p, e);
        } else if (match(p, TOK_LBRACKET)) {
            Expr *idx = expression(p);
            consume(p, TOK_RBRACKET, "expected ']' after index");
            e = expr_index(e, idx, p->previous.line);
        } else if (match(p, TOK_DOT)) {
            consume(p, TOK_IDENT, "expected property name after '.'");
            e = expr_get_prop(e, p->previous.start, p->previous.length,
                              p->previous.line);
        } else {
            break;
        }
    }
    return e;
}

static Expr *unary(Parser *p) {
    if (match(p, TOK_BANG) || match(p, TOK_MINUS)) {
        TokenType op = p->previous.type;
        int line = p->previous.line;
        Expr *operand = unary(p);
        return expr_unary(op, operand, line);
    }
    return call(p);
}

static Expr *factor(Parser *p) {
    Expr *e = unary(p);
    while (match(p, TOK_STAR) || match(p, TOK_SLASH) || match(p, TOK_PERCENT)) {
        TokenType op = p->previous.type;
        int line = p->previous.line;
        Expr *r = unary(p);
        e = expr_binary(op, e, r, line);
    }
    return e;
}

static Expr *term(Parser *p) {
    Expr *e = factor(p);
    while (match(p, TOK_PLUS) || match(p, TOK_MINUS)) {
        TokenType op = p->previous.type;
        int line = p->previous.line;
        Expr *r = factor(p);
        e = expr_binary(op, e, r, line);
    }
    return e;
}

static Expr *comparison(Parser *p) {
    Expr *e = term(p);
    while (match(p, TOK_LT) || match(p, TOK_LTEQ) ||
           match(p, TOK_GT) || match(p, TOK_GTEQ)) {
        TokenType op = p->previous.type;
        int line = p->previous.line;
        Expr *r = term(p);
        e = expr_binary(op, e, r, line);
    }
    return e;
}

static Expr *equality(Parser *p) {
    Expr *e = comparison(p);
    while (match(p, TOK_EQEQ) || match(p, TOK_BANGEQ)) {
        TokenType op = p->previous.type;
        int line = p->previous.line;
        Expr *r = comparison(p);
        e = expr_binary(op, e, r, line);
    }
    return e;
}

static Expr *assignment(Parser *p) {
    Expr *e = equality(p);
    if (match(p, TOK_EQ)) {
        int line = p->previous.line;
        Expr *value = assignment(p);
        if (e->kind == EXPR_VAR) {
            Expr *a = expr_assign(e->as.var.name, (int)strlen(e->as.var.name),
                                  value, line);
            expr_free(e);
            return a;
        }
        if (e->kind == EXPR_INDEX) {
            Expr *a = expr_index_assign(e->as.index.object, e->as.index.index,
                                        value, line);
            /* steal children; free shell */
            e->as.index.object = NULL;
            e->as.index.index = NULL;
            expr_free(e);
            return a;
        }
        if (e->kind == EXPR_GET_PROP) {
            Expr *a = expr_set_prop(e->as.get_prop.object,
                                    e->as.get_prop.name,
                                    (int)strlen(e->as.get_prop.name),
                                    value, line);
            e->as.get_prop.object = NULL;
            expr_free(e);
            return a;
        }
        error(p, "invalid assignment target");
        expr_free(value);
    }
    return e;
}

static Expr *expression(Parser *p) {
    return assignment(p);
}

/* ---- statements ---- */

static Stmt *out_statement(Parser *p) {
    int line = p->previous.line;
    Expr *e = expression(p);
    consume(p, TOK_SEMI, "expected ';' after out");
    return stmt_print(e, line);
}

static Stmt *ret_statement(Parser *p) {
    int line = p->previous.line;
    Expr *value = NULL;
    if (!check(p, TOK_SEMI)) {
        value = expression(p);
    }
    consume(p, TOK_SEMI, "expected ';' after ret");
    return stmt_return(value, line);
}

static Stmt *if_statement(Parser *p) {
    int line = p->previous.line;
    consume(p, TOK_LPAREN, "expected '(' after if/when");
    Expr *cond = expression(p);
    consume(p, TOK_RPAREN, "expected ')' after condition");
    Stmt *then_b = statement(p);
    Stmt *else_b = NULL;
    if (match(p, TOK_ELSE)) {
        else_b = statement(p);
    }
    return stmt_if(cond, then_b, else_b, line);
}

static Stmt *while_statement(Parser *p) {
    int line = p->previous.line;
    consume(p, TOK_LPAREN, "expected '(' after while/loop");
    Expr *cond = expression(p);
    consume(p, TOK_RPAREN, "expected ')' after condition");
    Stmt *body = statement(p);
    return stmt_while(cond, body, line);
}

static Stmt *for_statement(Parser *p) {
    int line = p->previous.line;
    consume(p, TOK_LPAREN, "expected '(' after 'for'");

    /* for (ident in expr)  OR  for (init; cond; incr) */
    if (check(p, TOK_IDENT)) {
        Token name = p->current;
        /* Look ahead: IDENT in → for-in. Need peek via advance carefully.
         * Save and check next token. */
        advance(p); /* consume IDENT */
        if (match(p, TOK_IN)) {
            Expr *iter = expression(p);
            consume(p, TOK_RPAREN, "expected ')' after for-in");
            Stmt *body = statement(p);
            return stmt_for_in(name.start, name.length, iter, body, line);
        }
        /* Not for-in: IDENT already consumed. Treat as C-style expr init. */
        Expr *lhs = expr_var(name.start, name.length, name.line);
        /* Continue parsing from current (which should be '=' or similar) */
        if (match(p, TOK_EQ)) {
            Expr *val = expression(p);
            Expr *as = expr_assign(name.start, name.length, val, line);
            expr_free(lhs);
            consume(p, TOK_SEMI, "expected ';' after for init");
            Stmt *init = stmt_expr(as, line);
            Expr *cond = NULL;
            if (!check(p, TOK_SEMI)) cond = expression(p);
            consume(p, TOK_SEMI, "expected ';' after for condition");
            Expr *incr = NULL;
            if (!check(p, TOK_RPAREN)) incr = expression(p);
            consume(p, TOK_RPAREN, "expected ')' after for clauses");
            Stmt *body = statement(p);
            return stmt_for(init, cond, incr, body, line);
        }
        expr_free(lhs);
        error(p, "expected 'in' or assignment in for");
        return stmt_expr(expr_lit_nil(line), line);
    }

    Stmt *init = NULL;
    if (match(p, TOK_LET)) {
        init = let_declaration(p); /* consumes its own ';' */
    } else if (match(p, TOK_SEMI)) {
        init = NULL;
    } else {
        Expr *e = expression(p);
        consume(p, TOK_SEMI, "expected ';' after for init");
        init = stmt_expr(e, line);
    }

    Expr *cond = NULL;
    if (!check(p, TOK_SEMI)) {
        cond = expression(p);
    }
    consume(p, TOK_SEMI, "expected ';' after for condition");

    Expr *incr = NULL;
    if (!check(p, TOK_RPAREN)) {
        incr = expression(p);
    }
    consume(p, TOK_RPAREN, "expected ')' after for clauses");
    Stmt *body = statement(p);
    return stmt_for(init, cond, incr, body, line);
}

static Stmt *block_stmt(Parser *p) {
    int line = p->previous.line;
    Stmt **list = NULL;
    int count = 0, cap = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (count + 1 > cap) {
            cap = cap < 8 ? 8 : cap * 2;
            list = realloc(list, (size_t)cap * sizeof(Stmt *));
            if (!list) { fprintf(stderr, "oom\n"); exit(1); }
        }
        list[count++] = declaration(p);
    }
    consume(p, TOK_RBRACE, "expected '}' after block");
    return stmt_block(list, count, line);
}

static Stmt *expr_statement(Parser *p) {
    int line = p->current.line;
    Expr *e = expression(p);
    consume(p, TOK_SEMI, "expected ';' after expression");
    return stmt_expr(e, line);
}

static Stmt *statement(Parser *p) {
    if (match(p, TOK_OUT))   return out_statement(p);
    if (match(p, TOK_RET))   return ret_statement(p);
    if (match(p, TOK_WHEN) || match(p, TOK_IF)) return if_statement(p);
    if (match(p, TOK_LOOP) || match(p, TOK_WHILE)) return while_statement(p);
    if (match(p, TOK_FOR))   return for_statement(p);
    if (match(p, TOK_LBRACE)) return block_stmt(p);
    return expr_statement(p);
}

static Stmt *let_declaration(Parser *p) {
    int line = p->previous.line;
    consume(p, TOK_IDENT, "expected variable name");
    Token name = p->previous;
    Expr *init = NULL;
    if (match(p, TOK_EQ)) {
        init = expression(p);
    } else {
        init = expr_lit_nil(line);
    }
    consume(p, TOK_SEMI, "expected ';' after variable declaration");
    return stmt_let(name.start, name.length, init, line);
}

static Stmt *fn_declaration(Parser *p) {
    int line = p->previous.line;
    consume(p, TOK_IDENT, "expected function name");
    Token name = p->previous;

    consume(p, TOK_LPAREN, "expected '(' after function name");
    char params[MAX_PARAMS][MAX_NAME];
    int arity = 0;
    if (!check(p, TOK_RPAREN)) {
        do {
            if (arity >= MAX_PARAMS) {
                error_current(p, "too many parameters");
                break;
            }
            consume(p, TOK_IDENT, "expected parameter name");
            int len = p->previous.length;
            if (len >= MAX_NAME) len = MAX_NAME - 1;
            memcpy(params[arity], p->previous.start, (size_t)len);
            params[arity][len] = '\0';
            arity++;
        } while (match(p, TOK_COMMA));
    }
    consume(p, TOK_RPAREN, "expected ')' after parameters");
    consume(p, TOK_LBRACE, "expected '{' before function body");
    Stmt *body = block_stmt(p);
    return stmt_fn(name.start, name.length, params, arity, body, line);
}

static Stmt *struct_declaration(Parser *p) {
    int line = p->previous.line;
    consume(p, TOK_IDENT, "expected struct name");
    Token name = p->previous;
    consume(p, TOK_LBRACE, "expected '{' after struct name");
    char fields[MAX_FIELDS][MAX_NAME];
    int count = 0;
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (count >= MAX_FIELDS) {
            error_current(p, "too many fields");
            break;
        }
        consume(p, TOK_IDENT, "expected field name");
        int len = p->previous.length;
        if (len >= MAX_NAME) len = MAX_NAME - 1;
        memcpy(fields[count], p->previous.start, (size_t)len);
        fields[count][len] = '\0';
        count++;
        /* optional semicolon or comma between fields */
        if (match(p, TOK_SEMI) || match(p, TOK_COMMA)) {
            /* continue */
        }
    }
    consume(p, TOK_RBRACE, "expected '}' after struct fields");
    /* optional trailing semicolon */
    match(p, TOK_SEMI);
    return stmt_struct(name.start, name.length, fields, count, line);
}

static Stmt *import_declaration(Parser *p) {
    int line = p->previous.line;
    if (match(p, TOK_STRING)) {
        int out_len = 0;
        char *path = unescape_string(p->previous.start, p->previous.length, &out_len);
        /* module name = basename without .my */
        const char *base = path;
        for (const char *s = path; *s; s++)
            if (*s == '/' || *s == '\\') base = s + 1;
        char module[MAX_NAME];
        int mlen = 0;
        while (base[mlen] && base[mlen] != '.' && mlen < MAX_NAME - 1) {
            module[mlen] = base[mlen];
            mlen++;
        }
        module[mlen] = '\0';
        consume(p, TOK_SEMI, "expected ';' after import");
        Stmt *s = stmt_import(module, mlen, path, out_len, true, line);
        free(path);
        return s;
    }
    consume(p, TOK_IDENT, "expected module name or string path");
    Token name = p->previous;
    consume(p, TOK_SEMI, "expected ';' after import");
    return stmt_import(name.start, name.length, name.start, name.length, false, line);
}

static Stmt *declaration(Parser *p) {
    Stmt *s;
    if (match(p, TOK_LET)) s = let_declaration(p);
    else if (match(p, TOK_FN)) s = fn_declaration(p);
    else if (match(p, TOK_STRUCT)) s = struct_declaration(p);
    else if (match(p, TOK_IMPORT)) s = import_declaration(p);
    else s = statement(p);

    if (p->panic_mode) synchronize(p);
    return s;
}

bool parse(const char *source, const char *path, Program *out) {
    Parser p;
    lexer_init(&p.lexer, source);
    p.path = path;
    p.had_error = false;
    p.panic_mode = false;
    advance(&p);

    Stmt **list = NULL;
    int count = 0, cap = 0;
    while (!check(&p, TOK_EOF)) {
        if (count + 1 > cap) {
            cap = cap < 8 ? 8 : cap * 2;
            list = realloc(list, (size_t)cap * sizeof(Stmt *));
            if (!list) { fprintf(stderr, "oom\n"); exit(1); }
        }
        list[count++] = declaration(&p);
    }

    out->stmts = list;
    out->count = count;
    return !p.had_error;
}
