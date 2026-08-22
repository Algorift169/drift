#include <stddef.h>

#include "drift/operator.h"

Value operator_apply_identity(const Value *left, const Value *right)
{
    if (left == NULL || right == NULL) {
        return value_create_boolean(0);
    }

    return value_create_boolean(left->type == right->type &&
                                left->identity != NULL &&
                                left->identity == right->identity);
}