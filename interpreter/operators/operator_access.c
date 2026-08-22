#include <stdio.h>
#include <stdlib.h>

#include "drift/operator.h"
#include "drift/array_value.h"

Value operator_apply(OperatorType op, const Value *container, const Value *index)
{
    Value result = value_create_null();

    if (op == OPERATOR_RANGE) {
        if (container == NULL || index == NULL) {
            fprintf(stderr, "Runtime Error: Range operator requires start and end values.\n");
            return result;
        }

        if (container->type == VALUE_INTEGER && index->type == VALUE_INTEGER) {
            long start = container->integer_value;
            long end = index->integer_value;
            size_t count = 0;
            Value *elements = NULL;

            if (start <= end) {
                count = (size_t)(end - start + 1);
                elements = (Value *)malloc(count * sizeof(Value));
                if (elements != NULL) {
                    for (size_t i = 0; i < count; i++) {
                        elements[i] = value_create_integer(start + (long)i);
                    }
                }
            } else {
                count = (size_t)(start - end + 1);
                elements = (Value *)malloc(count * sizeof(Value));
                if (elements != NULL) {
                    for (size_t i = 0; i < count; i++) {
                        elements[i] = value_create_integer(start - (long)i);
                    }
                }
            }

            if (elements != NULL) {
                ArrayValue *arr = array_value_create_dynamic_from_values(VALUE_INTEGER, elements, count);
                if (arr != NULL) {
                    result = value_create_array(arr);
                }
                for (size_t i = 0; i < count; i++) {
                    value_free(&elements[i]);
                }
                free(elements);
            } else {
                fprintf(stderr, "Runtime Error: Out of memory creating range.\n");
            }
        } else {
            fprintf(stderr, "Runtime Error: Range operator requires integer operands.\n");
        }
    }

    return result;
}