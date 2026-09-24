#ifndef MYC_LEXER_H
#define MYC_LEXER_H

#include "common.h"

typedef enum {
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA, TOK_SEMI, TOK_COLON, TOK_DOT,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_EQEQ, TOK_BANGEQ,
    TOK_LT, TOK_LTEQ, TOK_GT, TOK_GTEQ,
    TOK_BANG,
    TOK_IDENT, TOK_NUMBER, TOK_STRING,
    /* keywords */
    TOK_LET, TOK_FN, TOK_WHEN, TOK_ELSE, TOK_LOOP, TOK_RET, TOK_OUT,
    TOK_IF, TOK_WHILE, TOK_FOR, TOK_IN,
    TOK_TRUE, TOK_FALSE, TOK_NIL,
    TOK_STRUCT, TOK_IMPORT,
    TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
    int column; /* 1-based; 0 if unknown */
} Token;

typedef struct {
    const char *source;
    const char *current;
    const char *line_start;
    int line;
} Lexer;

void lexer_init(Lexer *lex, const char *source);
Token lexer_next(Lexer *lex);

#endif
