#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "drift/operator.h"

static int require_numeric_pair(const Value *left, const Value *right, double *l, double *r)
{
    if (left == NULL || right == NULL || l == NULL || r == NULL) {
        return 0;
    }

    if (left->type == VALUE_INTEGER) {
        *l = (double)left->integer_value;
    } else if (left->type == VALUE_FLOAT) {
        *l = left->float_value;
    } else if (left->type == VALUE_BOOLEAN) {
        *l = left->boolean_value ? 1.0 : 0.0;
    } else {
        return 0;
    }

    if (right->type == VALUE_INTEGER) {
        *r = (double)right->integer_value;
    } else if (right->type == VALUE_FLOAT) {
        *r = right->float_value;
    } else if (right->type == VALUE_BOOLEAN) {
        *r = right->boolean_value ? 1.0 : 0.0;
    } else {
        return 0;
    }

    return 1;
}

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    Value result = value_create_null();

    if (left == NULL || right == NULL) {
        fprintf(stderr, "Runtime Error: Arithmetic operators require numeric values.\n");
        return result;
    }

    if (left->type == VALUE_INTEGER && right->type == VALUE_INTEGER) {
        long lval = left->integer_value;
        long rval = right->integer_value;

        if (op == OPERATOR_ADD) {
            result = value_create_integer(lval + rval);
        } else if (op == OPERATOR_SUBTRACT) {
            result = value_create_integer(lval - rval);
        } else if (op == OPERATOR_MULTIPLY) {
            result = value_create_integer(lval * rval);
        } else if (op == OPERATOR_DIVIDE) {
            if (rval == 0) {
                fprintf(stderr, "Runtime Error: Division by zero.\n");
                return result;
            }
            if (lval % rval == 0) {
                result = value_create_integer(lval / rval);
            } else {
                result = value_create_float((double)lval / (double)rval);
            }
        } else if (op == OPERATOR_MODULO) {
            if (rval == 0) {
                fprintf(stderr, "Runtime Error: Modulo by zero.\n");
                return result;
            }
            result = value_create_integer(lval % rval);
        }
        return result;
    }

    double lhs = 0.0;
    double rhs = 0.0;

    if (op == OPERATOR_ADD || op == OPERATOR_SUBTRACT || op == OPERATOR_MULTIPLY ||
        op == OPERATOR_DIVIDE || op == OPERATOR_MODULO) {
        if (!require_numeric_pair(left, right, &lhs, &rhs)) {
            fprintf(stderr, "Runtime Error: Arithmetic operators require numeric values.\n");
            return result;
        }

        if (op == OPERATOR_ADD) {
            result = value_create_float(lhs + rhs);
        } else if (op == OPERATOR_SUBTRACT) {
            result = value_create_float(lhs - rhs);
        } else if (op == OPERATOR_MULTIPLY) {
            result = value_create_float(lhs * rhs);
        } else if (op == OPERATOR_DIVIDE) {
            if (rhs == 0.0) {
                fprintf(stderr, "Runtime Error: Division by zero.\n");
                return result;
            }
            result = value_create_float(lhs / rhs);
        } else if (op == OPERATOR_MODULO) {
            if (rhs == 0.0) {
                fprintf(stderr, "Runtime Error: Modulo by zero.\n");
                return result;
            }
            result = value_create_float(fmod(lhs, rhs));
        }
    }

    return result;
}

int operator_is_comparison(OperatorType op)
{
    return op == OPERATOR_EQUAL_EQUAL || op == OPERATOR_NOT_EQUAL ||
           op == OPERATOR_GREATER || op == OPERATOR_LESS ||
           op == OPERATOR_GREATER_EQUAL || op == OPERATOR_LESS_EQUAL;
}

int operator_is_logical(OperatorType op)
{
    return op == OPERATOR_AND_AND || op == OPERATOR_OR_OR || op == OPERATOR_NOT;
}

int operator_is_bitwise(OperatorType op)
{
    return op == OPERATOR_BITWISE_AND || op == OPERATOR_BITWISE_OR || op == OPERATOR_BITWISE_XOR ||
           op == OPERATOR_BITWISE_NOT || op == OPERATOR_SHIFT_LEFT || op == OPERATOR_SHIFT_RIGHT;
}
