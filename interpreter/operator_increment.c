#include <stdio.h>

#include "drift/operator.h"

Value operator_apply(OperatorType op, const Value *value, const Value *dummy)
{
    Value result = value_create_null();

    (void)dummy;

    if (value == NULL) {
        fprintf(stderr, "Runtime Error: Increment/decrement operators require an operand.\n");
        return result;
    }

    if (op == OPERATOR_INCREMENT) {
        if (value->type == VALUE_INTEGER) {
            result = value_create_integer(value->integer_value + 1);
        } else if (value->type == VALUE_FLOAT) {
            result = value_create_float(value->float_value + 1.0);
        } else if (value->type == VALUE_BOOLEAN) {
            result = value_create_boolean(value->boolean_value ? 1 : 0);
        } else {
            fprintf(stderr, "Runtime Error: Increment operator requires a numeric value.\n");
            return result;
        }
    } else if (op == OPERATOR_DECREMENT) {
        if (value->type == VALUE_INTEGER) {
            result = value_create_integer(value->integer_value - 1);
        } else if (value->type == VALUE_FLOAT) {
            result = value_create_float(value->float_value - 1.0);
        } else if (value->type == VALUE_BOOLEAN) {
            result = value_create_boolean(!value->boolean_value);
        } else {
            fprintf(stderr, "Runtime Error: Decrement operator requires a numeric value.\n");
            return result;
        }
    }

    return result;
}
