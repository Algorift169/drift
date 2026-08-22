#include <string.h>

#include "drift/identity_keywords.h"

TokenType identity_keyword_token_type(const char *value)
{
    if (value != NULL && strcmp(value, "is") == 0) {
        return TOKEN_IS;
    }

    return TOKEN_UNKNOWN;
}