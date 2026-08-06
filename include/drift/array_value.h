#ifndef DRIFT_ARRAY_VALUE_H
#define DRIFT_ARRAY_VALUE_H

#include <stddef.h>
#include "drift/value.h"

typedef struct ArrayValue {
    ValueType element_type;
    size_t dimension_count;
    long *dimensions;
    size_t total_count;
    Value *elements;
    int is_dynamic;
    size_t length;
} ArrayValue;

ArrayValue *array_value_create_empty(void);
ArrayValue *array_value_create_fixed(size_t dimension_count, const long *dimensions, ValueType element_type);
ArrayValue *array_value_create_from_values(size_t dimension_count, const long *dimensions, ValueType element_type, Value *values, size_t values_count);
ArrayValue *array_value_create_dynamic_from_values(ValueType element_type, const Value *values, size_t values_count);
ArrayValue *array_value_copy(const ArrayValue *source);
void array_value_free(ArrayValue *array);
int array_value_get_flat_index(const ArrayValue *array, const long *indices, size_t index_count, size_t *out_index);
const Value *array_value_get_element(const ArrayValue *array, const long *indices, size_t index_count, int *error);

#endif
