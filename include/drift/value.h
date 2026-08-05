#ifndef DRIFT_VALUE_H
#define DRIFT_VALUE_H

typedef enum {
    VALUE_INTEGER,
    VALUE_FLOAT,
    VALUE_STRING,
    VALUE_BOOLEAN,
    VALUE_NULL,
    VALUE_INFINITY
} ValueType;

typedef struct {
    ValueType type;
    long integer_value;
    double float_value;
    char *string_value;
    int boolean_value;
} Value;

Value value_create_integer(long value);
Value value_create_float(double value);
Value value_create_string(const char *value);
Value value_create_boolean(int value);
Value value_create_null(void);
Value value_create_infinity(void);
Value value_copy(const Value *value);
void value_free(Value *value);

#endif
