#include <stdio.h>

#include "drift/operator.h"

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    Value result = value_create_null();

    if (op == OPERATOR_ADD_ASSIGN || op == OPERATOR_SUBTRACT_ASSIGN ||
        op == OPERATOR_MULTIPLY_ASSIGN || op == OPERATOR_DIVIDE_ASSIGN ||
        op == OPERATOR_MODULO_ASSIGN) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Assignment operators require operands.\n");
            return result;
        }
        // Note: Assignment is handled at parser/interpreter level
        // This function is for evaluation context only
        result = *left;
    } else if (op == OPERATOR_AND_ASSIGN || op == OPERATOR_OR_ASSIGN ||
               op == OPERATOR_XOR_ASSIGN || op == OPERATOR_SHIFT_LEFT_ASSIGN ||
               op == OPERATOR_SHIFT_RIGHT_ASSIGN) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Bitwise assignment operators require operands.\n");
            return result;
        }
        // Note: Assignment is handled at parser/interpreter level
        result = *left;
    } else if (op == OPERATOR_ASSIGN) {
        if (right == NULL) {
            fprintf(stderr, "Runtime Error: Assignment requires a value.\n");
            return result;
        }
        result = *right;
    }

    return result;
}
