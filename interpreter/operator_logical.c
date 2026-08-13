#include <stdio.h>

#include "drift/operator.h"

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    Value result = value_create_boolean(0);

    if (left == NULL) {
        fprintf(stderr, "Runtime Error: Logical operators require a left operand.\n");
        return result;
    }

    if (op == OPERATOR_NOT) {
        if (left->type == VALUE_BOOLEAN) {
            result = value_create_boolean(!left->boolean_value);
        } else {
            result = value_create_boolean(!left->integer_value);
        }
    } else if (right != NULL) {
        int lhs = 0;
        int rhs = 0;

        if (left->type == VALUE_BOOLEAN) {
            lhs = left->boolean_value;
        } else {
            lhs = (left->type == VALUE_INTEGER && left->integer_value != 0) || (left->type == VALUE_FLOAT && left->float_value != 0.0);
        }

        if (right->type == VALUE_BOOLEAN) {
            rhs = right->boolean_value;
        } else {
            rhs = (right->type == VALUE_INTEGER && right->integer_value != 0) || (right->type == VALUE_FLOAT && right->float_value != 0.0);
        }

        if (op == OPERATOR_AND_AND) {
            result = value_create_boolean(lhs && rhs);
        } else if (op == OPERATOR_OR_OR) {
            result = value_create_boolean(lhs || rhs);
        }
    }

    return result;
}
