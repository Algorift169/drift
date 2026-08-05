#include <stdlib.h>
#include <string.h>

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

    free(value->string_value);
    value->string_value = NULL;
    value->type = VALUE_STRING;
    value->integer_value = 0;
    value->float_value = 0.0;
    value->boolean_value = 0;
}
