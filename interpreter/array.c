#include <stdio.h>
#include <stdlib.h>

#include "drift/array.h"
#include "drift/array_value.h"
#include "drift/interpreter.h"
#include "drift/value.h"

static void print_value(const Value *value);

static void print_array_recursive(const ArrayValue *array, size_t dimension, size_t base_offset)
{
    if (dimension + 1 == array->dimension_count) {
        for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
            if (i > 0) {
                printf(" ");
            }
            print_value(&array->elements[base_offset + i]);
        }
        return;
    }

    size_t stride = 1;
    for (size_t i = dimension + 1; i < array->dimension_count; ++i) {
        stride *= (size_t)array->dimensions[i];
    }

    for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
        if (i > 0) {
            printf("\n");
            if (dimension == 0 && array->dimension_count > 2) {
                printf("\n");
            }
        }
        print_array_recursive(array, dimension + 1, base_offset + i * stride);
    }
}

void print_array_value(const ArrayValue *array)
{
    if (array == NULL) {
        return;
    }

    if (array->is_dynamic && array->length == 0) {
        return;
    }

    if (array->dimension_count == 1) {
        for (size_t i = 0; i < array->length; ++i) {
            if (i > 0) {
                printf(",");
            }
            print_value(&array->elements[i]);
        }
        return;
    }

    print_array_recursive(array, 0, 0);
}

void print_array_element(const ArrayValue *array, const long *indices, size_t index_count)
{
    if (array == NULL) {
        return;
    }

    if (index_count != array->dimension_count) {
        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", index_count, array->dimension_count);
        return;
    }

    int error = 0;
    const Value *value = array_value_get_element(array, indices, index_count, &error);
    if (value == NULL) {
        if (error) {
            if (index_count == 0) {
                fprintf(stderr, "Runtime Error: Invalid array access.\n");
            } else {
                fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
            }
        }
        return;
    }
    print_value(value);
}

void print_array_selection(const ArrayValue *array, const ArrayAccess *access)
{
    if (array == NULL || access == NULL || !access->is_selection) {
        return;
    }

    if (access->selection_tuple_size != array->dimension_count) {
        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", access->selection_tuple_size, array->dimension_count);
        return;
    }

    for (size_t i = 0; i < access->selection_count; ++i) {
        const long *indices = access->selection_indices + i * access->selection_tuple_size;
        int error = 0;
        const Value *value = array_value_get_element(array, indices, access->selection_tuple_size, &error);
        if (value == NULL) {
            if (error) {
                fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
            }
            return;
        }

        if (i > 0) {
            printf(" ");
        }
        print_value(value);
    }
}

static void print_value(const Value *value)
{
    if (value == NULL) {
        return;
    }

    switch (value->type) {
        case VALUE_INTEGER:
            printf("%ld", value->integer_value);
            break;
        case VALUE_FLOAT:
            printf("%g", value->float_value);
            break;
        case VALUE_STRING:
            printf("%s", value->string_value ? value->string_value : "");
            break;
        case VALUE_BOOLEAN:
            printf(value->boolean_value ? "true" : "false");
            break;
        case VALUE_NULL:
            printf("null");
            break;
        case VALUE_INFINITY:
            printf("infinity");
            break;
        case VALUE_ARRAY:
            print_array_value(value->array_value);
            break;
        default:
            break;
    }
}
