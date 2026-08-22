#include <stdio.h>
#include <string.h>

#include "drift/operator.h"
#include "drift/array_value.h"

Value operator_apply(OperatorType op, const Value *container, const Value *member)
{
    Value result = value_create_boolean(0);

    if (op == OPERATOR_IN) {
        if (container == NULL) {
            fprintf(stderr, "Runtime Error: Membership operator requires a container.\n");
            return result;
        }

        if (container->type == VALUE_ARRAY && member != NULL) {
            ArrayValue *arr = container->array_value;
            if (arr != NULL) {
                for (size_t i = 0; i < arr->length; i++) {
                    Value *elem = &arr->elements[i];

                    if (member->type == VALUE_INTEGER && elem->type == VALUE_INTEGER) {
                        if (member->integer_value == elem->integer_value) {
                            return value_create_boolean(1);
                        }
                    } else if (member->type == VALUE_STRING && elem->type == VALUE_STRING) {
                        const char *member_str = member->string_value ? member->string_value : "";
                        const char *elem_str = elem->string_value ? elem->string_value : "";
                        if (strcmp(member_str, elem_str) == 0) {
                            return value_create_boolean(1);
                        }
                    } else if (member->type == VALUE_FLOAT && elem->type == VALUE_FLOAT) {
                        if (member->float_value == elem->float_value) {
                            return value_create_boolean(1);
                        }
                    }
                }
            }
        } else if (container->type == VALUE_STRING && member != NULL) {
            if (member->type == VALUE_STRING) {
                const char *container_str = container->string_value ? container->string_value : "";
                const char *member_str = member->string_value ? member->string_value : "";
                if (strstr(container_str, member_str) != NULL) {
                    return value_create_boolean(1);
                }
            }
        }
    }

    return result;
}
