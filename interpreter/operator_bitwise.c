#include <stdio.h>
#include <math.h>

#include "drift/operator.h"

static int require_integer_value(const Value *val, long *out)
{
    if (val == NULL || out == NULL) {
        return 0;
    }
    if (val->type == VALUE_INTEGER) {
        *out = val->integer_value;
        return 1;
    }
    if (val->type == VALUE_FLOAT) {
        if (floor(val->float_value) == val->float_value) {
            *out = (long)val->float_value;
            return 1;
        }
    }
    return 0;
}

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    Value result = value_create_null();
    long lval = 0;
    long rval = 0;

    if (op == OPERATOR_BITWISE_NOT) {
        if (left == NULL) {
            fprintf(stderr, "Runtime Error: Bitwise NOT requires an operand.\n");
            return result;
        }
        if (require_integer_value(left, &lval)) {
            result = value_create_integer(~lval);
        } else {
            fprintf(stderr, "Runtime Error: Bitwise NOT requires an integer.\n");
        }
    } else {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Bitwise operators require two operands.\n");
            return result;
        }
        if (!require_integer_value(left, &lval) || !require_integer_value(right, &rval)) {
            fprintf(stderr, "Runtime Error: Bitwise operators require integers.\n");
            return result;
        }

        if (op == OPERATOR_BITWISE_AND) {
            result = value_create_integer(lval & rval);
        } else if (op == OPERATOR_BITWISE_OR) {
            result = value_create_integer(lval | rval);
        } else if (op == OPERATOR_BITWISE_XOR) {
            result = value_create_integer(lval ^ rval);
        } else if (op == OPERATOR_SHIFT_LEFT) {
            result = value_create_integer(lval << rval);
        } else if (op == OPERATOR_SHIFT_RIGHT) {
            result = value_create_integer(lval >> rval);
        }
    }

    return result;
}
