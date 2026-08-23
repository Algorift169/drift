#include <stddef.h>

#include "drift/operator_ternary.h"

Value operator_apply_ternary(const Value *condition, const Value *true_value, const Value *false_value)
{
    int is_truthy = 0;

    if (condition != NULL) {
        if (condition->type == VALUE_BOOLEAN) {
            is_truthy = condition->boolean_value;
        } else if (condition->type == VALUE_INTEGER) {
            is_truthy = condition->integer_value != 0;
        } else if (condition->type == VALUE_FLOAT) {
            is_truthy = condition->float_value != 0.0;
        } else if (condition->type == VALUE_STRING) {
            is_truthy = condition->string_value != NULL && condition->string_value[0] != '\0';
        } else if (condition->type == VALUE_ARRAY) {
            is_truthy = condition->array_value != NULL;
        } else if (condition->type == VALUE_INFINITY) {
            is_truthy = 1;
        }
    }

    if (is_truthy) {
        return value_copy(true_value);
    }

    return value_copy(false_value);
}