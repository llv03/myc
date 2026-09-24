#include "lexer.h"

void lexer_init(Lexer *lex, const char *source) {
    lex->source = source;
    lex->current = source;
    lex->line_start = source;
    lex->line = 1;
}

static bool is_at_end(Lexer *lex) {
    return *lex->current == '\0';
}

static char peek(Lexer *lex) {
    return *lex->current;
}

static char peek_next(Lexer *lex) {
    if (is_at_end(lex)) return '\0';
    return lex->current[1];
}

static char advance(Lexer *lex) {
    return *lex->current++;
}

static bool match(Lexer *lex, char expected) {
    if (is_at_end(lex) || *lex->current != expected) return false;
    lex->current++;
    return true;
}

static Token make_token(Lexer *lex, TokenType type, const char *start) {
    Token t;
    t.type = type;
    t.start = start;
    t.length = (int)(lex->current - start);
    t.line = lex->line;
    t.column = (int)(start - lex->line_start) + 1;
    return t;
}

static Token error_token(Lexer *lex, const char *msg) {
    Token t;
    t.type = TOK_ERROR;
    t.start = msg;
    t.length = (int)strlen(msg);
    t.line = lex->line;
    t.column = (int)(lex->current - lex->line_start) + 1;
    return t;
}

static void skip_whitespace(Lexer *lex) {
    for (;;) {
        char c = peek(lex);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lex);
                break;
            case '\n':
                advance(lex);
                lex->line++;
                lex->line_start = lex->current;
                break;
            case '/':
                if (peek_next(lex) == '/') {
                    while (peek(lex) != '\n' && !is_at_end(lex)) advance(lex);
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static TokenType check_keyword(const char *start, int length,
                               int kw_start, int kw_len,
                               const char *rest, TokenType type) {
    if (length == kw_start + kw_len &&
        memcmp(start + kw_start, rest, (size_t)kw_len) == 0) {
        return type;
    }
    return TOK_IDENT;
}

static TokenType ident_type(const char *start, int length) {
    switch (start[0]) {
        case 'e': return check_keyword(start, length, 1, 3, "lse", TOK_ELSE);
        case 'f':
            if (length == 2 && start[1] == 'n') return TOK_FN;
            if (length == 3 && memcmp(start, "for", 3) == 0) return TOK_FOR;
            if (length == 5 && memcmp(start, "false", 5) == 0) return TOK_FALSE;
            break;
        case 'i':
            if (length == 2 && start[1] == 'f') return TOK_IF;
            if (length == 2 && start[1] == 'n') return TOK_IN;
            if (length == 6 && memcmp(start, "import", 6) == 0) return TOK_IMPORT;
            break;
        case 'l':
            if (length == 3 && memcmp(start, "let", 3) == 0) return TOK_LET;
            if (length == 4 && memcmp(start, "loop", 4) == 0) return TOK_LOOP;
            break;
        case 'n': return check_keyword(start, length, 1, 2, "il", TOK_NIL);
        case 'o': return check_keyword(start, length, 1, 2, "ut", TOK_OUT);
        case 'r': return check_keyword(start, length, 1, 2, "et", TOK_RET);
        case 's': return check_keyword(start, length, 1, 5, "truct", TOK_STRUCT);
        case 't': return check_keyword(start, length, 1, 3, "rue", TOK_TRUE);
        case 'w':
            if (length == 4 && memcmp(start, "when", 4) == 0) return TOK_WHEN;
            if (length == 5 && memcmp(start, "while", 5) == 0) return TOK_WHILE;
            break;
    }
    return TOK_IDENT;
}

static Token identifier(Lexer *lex, const char *start) {
    while (is_alpha(peek(lex)) || is_digit(peek(lex))) advance(lex);
    return make_token(lex, ident_type(start, (int)(lex->current - start)), start);
}

static Token number(Lexer *lex, const char *start) {
    while (is_digit(peek(lex))) advance(lex);
    if (peek(lex) == '.' && is_digit(peek_next(lex))) {
        advance(lex); /* '.' */
        while (is_digit(peek(lex))) advance(lex);
    }
    return make_token(lex, TOK_NUMBER, start);
}

static Token string_lit(Lexer *lex, const char *start) {
    /* opening " already consumed */
    while (peek(lex) != '"' && !is_at_end(lex)) {
        if (peek(lex) == '\n') {
            advance(lex);
            lex->line++;
            lex->line_start = lex->current;
            continue;
        }
        if (peek(lex) == '\\') {
            advance(lex);
            if (!is_at_end(lex)) advance(lex);
        } else {
            advance(lex);
        }
    }
    if (is_at_end(lex)) return error_token(lex, "unterminated string");
    advance(lex); /* closing " */
    return make_token(lex, TOK_STRING, start);
}

Token lexer_next(Lexer *lex) {
    skip_whitespace(lex);
    const char *start = lex->current;

    if (is_at_end(lex)) return make_token(lex, TOK_EOF, start);

    char c = advance(lex);

    if (is_alpha(c)) return identifier(lex, start);
    if (is_digit(c)) return number(lex, start);

    switch (c) {
        case '(': return make_token(lex, TOK_LPAREN, start);
        case ')': return make_token(lex, TOK_RPAREN, start);
        case '{': return make_token(lex, TOK_LBRACE, start);
        case '}': return make_token(lex, TOK_RBRACE, start);
        case '[': return make_token(lex, TOK_LBRACKET, start);
        case ']': return make_token(lex, TOK_RBRACKET, start);
        case ',': return make_token(lex, TOK_COMMA, start);
        case ';': return make_token(lex, TOK_SEMI, start);
        case ':': return make_token(lex, TOK_COLON, start);
        case '.': return make_token(lex, TOK_DOT, start);
        case '+': return make_token(lex, TOK_PLUS, start);
        case '-': return make_token(lex, TOK_MINUS, start);
        case '*': return make_token(lex, TOK_STAR, start);
        case '%': return make_token(lex, TOK_PERCENT, start);
        case '/': return make_token(lex, TOK_SLASH, start);
        case '"': return string_lit(lex, start);
        case '!':
            return make_token(lex, match(lex, '=') ? TOK_BANGEQ : TOK_BANG, start);
        case '=':
            return make_token(lex, match(lex, '=') ? TOK_EQEQ : TOK_EQ, start);
        case '<':
            return make_token(lex, match(lex, '=') ? TOK_LTEQ : TOK_LT, start);
        case '>':
            return make_token(lex, match(lex, '=') ? TOK_GTEQ : TOK_GT, start);
    }

    return error_token(lex, "unexpected character");
}
