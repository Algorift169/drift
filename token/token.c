#include <stdlib.h>
#include <string.h>

#include "drift/token.h"

char *drift_duplicate_string(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    length = strlen(value);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length + 1U);
    return copy;
}

void token_free_array(Token *tokens, size_t count)
{
    size_t i;

    if (tokens == NULL) {
        return;
    }

    for (i = 0; i < count; ++i) {
        free(tokens[i].value);
    }

    free(tokens);
}
