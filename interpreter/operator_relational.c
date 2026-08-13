#include <stdio.h>
#include <string.h>

#include "drift/operator.h"

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    Value result = value_create_boolean(0);

    if (left == NULL || right == NULL) {
        fprintf(stderr, "Runtime Error: Comparison operators require left and right operands.\n");
        return result;
    }

    if (op == OPERATOR_EQUAL_EQUAL) {
        if (left->type == VALUE_STRING && right->type == VALUE_STRING) {
            result = value_create_boolean(strcmp(left->string_value ? left->string_value : "", right->string_value ? right->string_value : "") == 0);
        } else {
            result = value_create_boolean(left->type == right->type && left->integer_value == right->integer_value && left->float_value == right->float_value);
        }
    } else if (op == OPERATOR_NOT_EQUAL) {
        if (left->type == VALUE_STRING && right->type == VALUE_STRING) {
            result = value_create_boolean(strcmp(left->string_value ? left->string_value : "", right->string_value ? right->string_value : "") != 0);
        } else {
            result = value_create_boolean(!(left->type == right->type && left->integer_value == right->integer_value && left->float_value == right->float_value));
        }
    } else if (op == OPERATOR_GREATER) {
        result = value_create_boolean((left->type == VALUE_INTEGER && right->type == VALUE_INTEGER && left->integer_value > right->integer_value) ||
                                     (left->type == VALUE_FLOAT && right->type == VALUE_FLOAT && left->float_value > right->float_value));
    } else if (op == OPERATOR_LESS) {
        result = value_create_boolean((left->type == VALUE_INTEGER && right->type == VALUE_INTEGER && left->integer_value < right->integer_value) ||
                                     (left->type == VALUE_FLOAT && right->type == VALUE_FLOAT && left->float_value < right->float_value));
    } else if (op == OPERATOR_GREATER_EQUAL) {
        result = value_create_boolean((left->type == VALUE_INTEGER && right->type == VALUE_INTEGER && left->integer_value >= right->integer_value) ||
                                     (left->type == VALUE_FLOAT && right->type == VALUE_FLOAT && left->float_value >= right->float_value));
    } else if (op == OPERATOR_LESS_EQUAL) {
        result = value_create_boolean((left->type == VALUE_INTEGER && right->type == VALUE_INTEGER && left->integer_value <= right->integer_value) ||
                                     (left->type == VALUE_FLOAT && right->type == VALUE_FLOAT && left->float_value <= right->float_value));
    }

    return result;
}
