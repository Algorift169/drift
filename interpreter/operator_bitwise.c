#include <stdio.h>

#include "drift/operator.h"

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    Value result = value_create_integer(0);

    if (left == NULL || right == NULL) {
        fprintf(stderr, "Runtime Error: Bitwise operators require left and right operands.\n");
        return result;
    }

    if (left->type != VALUE_INTEGER || right->type != VALUE_INTEGER) {
        fprintf(stderr, "Runtime Error: Bitwise operators require integer values.\n");
        return result;
    }

    if (op == OPERATOR_BITWISE_AND) {
        result = value_create_integer(left->integer_value & right->integer_value);
    } else if (op == OPERATOR_BITWISE_OR) {
        result = value_create_integer(left->integer_value | right->integer_value);
    } else if (op == OPERATOR_BITWISE_XOR) {
        result = value_create_integer(left->integer_value ^ right->integer_value);
    } else if (op == OPERATOR_BITWISE_NOT) {
        result = value_create_integer(~left->integer_value);
    } else if (op == OPERATOR_SHIFT_LEFT) {
        result = value_create_integer(left->integer_value << right->integer_value);
    } else if (op == OPERATOR_SHIFT_RIGHT) {
        result = value_create_integer(left->integer_value >> right->integer_value);
    }

    return result;
}
