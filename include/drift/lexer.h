#ifndef DRIFT_LEXER_H
#define DRIFT_LEXER_H

#include <stddef.h>

#include "drift/token.h"

typedef struct {
    const char *source;
    size_t index;
    size_t length;
} Lexer;

Lexer lexer_create(const char *source);
Token *lexer_scan_all(Lexer *lexer, size_t *token_count);

#endif
