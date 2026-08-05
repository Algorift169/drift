#ifndef DRIFT_TOKEN_H
#define DRIFT_TOKEN_H

#include <stddef.h>

typedef enum {
    TOKEN_SAY,
    TOKEN_VAR,
    TOKEN_IDENTIFIER,
    TOKEN_EQUAL,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_INFINITY,
    TOKEN_NEWLINE,
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

char *drift_duplicate_string(const char *value);
void token_free_array(Token *tokens, size_t count);

#endif
