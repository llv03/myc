#ifndef MYC_PARSER_H
#define MYC_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    const char *path; /* source file path for diagnostics; may be NULL */
    bool had_error;
    bool panic_mode;
} Parser;

bool parse(const char *source, const char *path, Program *out);

#endif
