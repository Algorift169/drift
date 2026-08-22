#include <string.h>

#include "drift/logical_keywords.h"

TokenType logical_keyword_token_type(const char *value)
{
    if (value == NULL) {
        return TOKEN_UNKNOWN;
    }

    if (strcmp(value, "and") == 0) {
        return TOKEN_AND_AND;
    }

    if (strcmp(value, "or") == 0) {
        return TOKEN_OR_OR;
    }

    if (strcmp(value, "not") == 0) {
        return TOKEN_BANG;
    }

    return TOKEN_UNKNOWN;
}