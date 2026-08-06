#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array_value.h"
#include "drift/value.h"

Value value_create_integer(long value)
{
    Value result;
    result.type = VALUE_INTEGER;
    result.integer_value = value;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = 0;
    return result;
}

Value value_create_float(double value)
{
    Value result;
    result.type = VALUE_FLOAT;
    result.integer_value = 0;
    result.float_value = value;
    result.string_value = NULL;
    result.boolean_value = 0;
    return result;
}

Value value_create_string(const char *value)
{
    Value result;
    result.type = VALUE_STRING;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.boolean_value = 0;
    result.string_value = NULL;

    if (value != NULL) {
        size_t length = strlen(value);
        result.string_value = (char *)malloc(length + 1U);
        if (result.string_value != NULL) {
            memcpy(result.string_value, value, length + 1U);
        }
    }

    return result;
}

Value value_create_boolean(int value)
{
    Value result;
    result.type = VALUE_BOOLEAN;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = value;
    return result;
}

Value value_create_null(void)
{
    Value result;
    result.type = VALUE_NULL;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = 0;
    return result;
}

Value value_create_infinity(void)
{
    Value result;
    result.type = VALUE_INFINITY;
    result.integer_value = 0;
    result.float_value = INFINITY;
    result.string_value = NULL;
    result.boolean_value = 0;
    return result;
}

Value value_create_array(ArrayValue *value)
{
    Value result;
    result.type = VALUE_ARRAY;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = 0;
    result.array_value = value;
    return result;
}

Value value_copy(const Value *value)
{
    Value result;

    if (value == NULL) {
        return value_create_string(NULL);
    }

    switch (value->type) {
        case VALUE_INTEGER:
            result = value_create_integer(value->integer_value);
            break;
        case VALUE_FLOAT:
            result = value_create_float(value->float_value);
            break;
        case VALUE_STRING:
            result = value_create_string(value->string_value);
            break;
        case VALUE_BOOLEAN:
            result = value_create_boolean(value->boolean_value);
            break;
        case VALUE_NULL:
            result = value_create_null();
            break;
        case VALUE_INFINITY:
            result = value_create_infinity();
            break;
        case VALUE_ARRAY:
            result.type = VALUE_ARRAY;
            result.array_value = array_value_copy(value->array_value);
            result.integer_value = 0;
            result.float_value = 0.0;
            result.string_value = NULL;
            result.boolean_value = 0;
            break;
        default:
            result = value_create_string(NULL);
            break;
    }

    return result;
}

void value_free(Value *value)
{
    if (value == NULL) {
        return;
    }

    if (value->type == VALUE_ARRAY) {
        array_value_free(value->array_value);
        value->array_value = NULL;
    }

    free(value->string_value);
    value->string_value = NULL;
    value->type = VALUE_NULL;
    value->integer_value = 0;
    value->float_value = 0.0;
    value->boolean_value = 0;
}
