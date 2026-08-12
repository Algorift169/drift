#include <stdlib.h>
#include <string.h>

#include "drift/array_value.h"
#include "drift/value.h"

static size_t product_dimensions(size_t dimension_count, const long *dimensions)
{
    size_t result = 1;
    for (size_t i = 0; i < dimension_count; ++i) {
        if (dimensions[i] <= 0) {
            return 0;
        }
        result *= (size_t)dimensions[i];
    }
    return result;
}

ArrayValue *array_value_create_empty(void)
{
    ArrayValue *array = (ArrayValue *)malloc(sizeof(ArrayValue));
    if (array == NULL) {
        return NULL;
    }

    array->element_type = VALUE_NULL;
    array->dimension_count = 1;
    array->dimensions = (long *)malloc(sizeof(long));
    if (array->dimensions == NULL) {
        free(array);
        return NULL;
    }
    array->dimensions[0] = 0;
    array->total_count = 0;
    array->elements = NULL;
    array->is_dynamic = 1;
    array->length = 0;
    return array;
}

ArrayValue *array_value_create_fixed(size_t dimension_count, const long *dimensions, ValueType element_type)
{
    ArrayValue *array = (ArrayValue *)malloc(sizeof(ArrayValue));
    if (array == NULL) {
        return NULL;
    }

    array->dimension_count = dimension_count;
    array->dimensions = (long *)malloc(dimension_count * sizeof(long));
    if (array->dimensions == NULL) {
        free(array);
        return NULL;
    }

    array->total_count = product_dimensions(dimension_count, dimensions);
    array->elements = (Value *)malloc(array->total_count * sizeof(Value));
    if (array->elements == NULL) {
        free(array->dimensions);
        free(array);
        return NULL;
    }

    array->element_type = element_type;
    array->is_dynamic = 0;
    array->length = array->total_count;

    for (size_t i = 0; i < dimension_count; ++i) {
        array->dimensions[i] = dimensions[i];
    }

    for (size_t i = 0; i < array->total_count; ++i) {
        array->elements[i] = value_create_null();
    }

    return array;
}

ArrayValue *array_value_create_from_values(size_t dimension_count, const long *dimensions, ValueType element_type, Value *values, size_t values_count)
{
    ArrayValue *array = (ArrayValue *)malloc(sizeof(ArrayValue));
    if (array == NULL) {
        return NULL;
    }

    array->dimension_count = dimension_count;
    array->dimensions = (long *)malloc(dimension_count * sizeof(long));
    if (array->dimensions == NULL) {
        free(array);
        return NULL;
    }

    array->total_count = product_dimensions(dimension_count, dimensions);
    if (array->total_count != values_count) {
        free(array->dimensions);
        free(array);
        return NULL;
    }

    array->elements = (Value *)malloc(values_count * sizeof(Value));
    if (array->elements == NULL) {
        free(array->dimensions);
        free(array);
        return NULL;
    }

    array->element_type = element_type;
    array->is_dynamic = 0;
    array->length = values_count;

    for (size_t i = 0; i < dimension_count; ++i) {
        array->dimensions[i] = dimensions[i];
    }

    for (size_t i = 0; i < values_count; ++i) {
        array->elements[i] = value_copy(&values[i]);
    }

    return array;
}

ArrayValue *array_value_create_dynamic_from_values(ValueType element_type, const Value *values, size_t values_count)
{
    ArrayValue *array = (ArrayValue *)malloc(sizeof(ArrayValue));
    if (array == NULL) {
        return NULL;
    }

    array->dimension_count = 1;
    array->dimensions = (long *)malloc(sizeof(long));
    if (array->dimensions == NULL) {
        free(array);
        return NULL;
    }

    array->dimensions[0] = (long)values_count;
    array->total_count = values_count;
    array->elements = (Value *)malloc(values_count * sizeof(Value));
    if (array->elements == NULL) {
        free(array->dimensions);
        free(array);
        return NULL;
    }

    array->element_type = element_type;
    array->is_dynamic = 1;
    array->length = values_count;

    for (size_t i = 0; i < values_count; ++i) {
        array->elements[i] = value_copy(&values[i]);
    }

    return array;
}

ArrayValue *array_value_copy(const ArrayValue *source)
{
    if (source == NULL) {
        return NULL;
    }

    ArrayValue *copy = (ArrayValue *)malloc(sizeof(ArrayValue));
    if (copy == NULL) {
        return NULL;
    }

    copy->dimension_count = source->dimension_count;
    copy->dimensions = (long *)malloc(source->dimension_count * sizeof(long));
    if (copy->dimensions == NULL) {
        free(copy);
        return NULL;
    }
    for (size_t i = 0; i < source->dimension_count; ++i) {
        copy->dimensions[i] = source->dimensions[i];
    }

    copy->total_count = source->total_count;
    copy->elements = (Value *)malloc(source->total_count * sizeof(Value));
    if (copy->elements == NULL) {
        free(copy->dimensions);
        free(copy);
        return NULL;
    }

    copy->element_type = source->element_type;
    copy->is_dynamic = source->is_dynamic;
    copy->length = source->length;

    for (size_t i = 0; i < source->total_count; ++i) {
        copy->elements[i] = value_copy(&source->elements[i]);
    }

    return copy;
}

void array_value_free(ArrayValue *array)
{
    if (array == NULL) {
        return;
    }

    free(array->dimensions);
    array->dimensions = NULL;

    if (array->elements != NULL) {
        for (size_t i = 0; i < array->total_count; ++i) {
            value_free(&array->elements[i]);
        }
        free(array->elements);
    }

    free(array);
}

int array_value_get_flat_index(const ArrayValue *array, const long *indices, size_t index_count, size_t *out_index)
{
    if (array == NULL || indices == NULL || out_index == NULL) {
        return 0;
    }

    if (index_count != array->dimension_count) {
        return 0;
    }

    size_t flat_index = 0;
    size_t stride = 1;

    for (size_t i = array->dimension_count; i > 0; --i) {
        size_t dim = i - 1;
        long index = indices[dim];
        if (index < 0 || index >= array->dimensions[dim]) {
            return 0;
        }
        flat_index += (size_t)index * stride;
        stride *= (size_t)array->dimensions[dim];
    }

    *out_index = flat_index;
    return 1;
}

const Value *array_value_get_element(const ArrayValue *array, const long *indices, size_t index_count, int *error)
{
    if (array == NULL || indices == NULL) {
        if (error != NULL) {
            *error = 1;
        }
        return NULL;
    }

    size_t flat_index;
    if (!array_value_get_flat_index(array, indices, index_count, &flat_index)) {
        if (error != NULL) {
            *error = 1;
        }
        return NULL;
    }

    if (error != NULL) {
        *error = 0;
    }
    return &array->elements[flat_index];
}

static int array_value_resize(ArrayValue *array, const long *new_dimensions)
{
    size_t new_total_count = 1;
    for (size_t i = 0; i < array->dimension_count; ++i) {
        if (new_dimensions[i] < 0) {
            return 0;
        }
        new_total_count *= (size_t)new_dimensions[i];
    }

    Value *new_elements = (Value *)malloc(new_total_count * sizeof(Value));
    if (new_elements == NULL && new_total_count > 0) {
        return 0;
    }

    for (size_t i = 0; i < new_total_count; ++i) {
        new_elements[i] = value_create_null();
    }

    if (array->total_count > 0 && array->elements != NULL) {
        long *coords = (long *)malloc(array->dimension_count * sizeof(long));
        if (coords == NULL) {
            free(new_elements);
            return 0;
        }

        for (size_t idx = 0; idx < array->total_count; ++idx) {
            size_t temp = idx;
            for (size_t i = array->dimension_count; i > 0; --i) {
                size_t dim = i - 1;
                coords[dim] = temp % array->dimensions[dim];
                temp /= array->dimensions[dim];
            }

            size_t new_flat_index = 0;
            size_t stride = 1;
            for (size_t i = array->dimension_count; i > 0; --i) {
                size_t dim = i - 1;
                new_flat_index += (size_t)coords[dim] * stride;
                stride *= (size_t)new_dimensions[dim];
            }

            new_elements[new_flat_index] = array->elements[idx];
        }
        free(coords);
        free(array->elements);
    }

    array->elements = new_elements;
    array->total_count = new_total_count;
    array->length = new_total_count;

    for (size_t i = 0; i < array->dimension_count; ++i) {
        array->dimensions[i] = new_dimensions[i];
    }

    return 1;
}

int array_value_set_element(ArrayValue *array, const long *indices, size_t index_count, const Value *value)
{
    if (array == NULL || value == NULL || value->type == VALUE_ARRAY) {
        return 0;
    }

    if (index_count != array->dimension_count) {
        return 0;
    }

    int need_resize = 0;
    for (size_t i = 0; i < array->dimension_count; ++i) {
        if (indices[i] < 0) {
            return 0;
        }
        if (indices[i] >= array->dimensions[i]) {
            if (!array->is_dynamic) {
                return 0;
            }
            need_resize = 1;
        }
    }

    if (need_resize) {
        long *new_dimensions = (long *)malloc(array->dimension_count * sizeof(long));
        if (new_dimensions == NULL) {
            return 0;
        }
        for (size_t i = 0; i < array->dimension_count; ++i) {
            new_dimensions[i] = array->dimensions[i];
            if (indices[i] >= array->dimensions[i]) {
                new_dimensions[i] = indices[i] + 1;
            }
        }
        if (!array_value_resize(array, new_dimensions)) {
            free(new_dimensions);
            return 0;
        }
        free(new_dimensions);
    }

    size_t flat_index = 0;
    if (!array_value_get_flat_index(array, indices, index_count, &flat_index)) {
        return 0;
    }

    if (array->element_type != VALUE_NULL && value->type != VALUE_NULL && value->type != array->element_type) {
        return 0;
    }

    value_free(&array->elements[flat_index]);
    array->elements[flat_index] = value_copy(value);
    if (array->element_type == VALUE_NULL && value->type != VALUE_NULL) {
        array->element_type = value->type;
    }
    return 1;
}
