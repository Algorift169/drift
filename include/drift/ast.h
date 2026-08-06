#ifndef DRIFT_AST_H
#define DRIFT_AST_H

#include "drift/value.h"
#include "drift/array.h"

typedef struct {
    char *name;
    Value value;
    int is_assignment;
    int is_array_expression;
    int is_array_declared;
    ArrayAccess array_access;
} VariableDeclaration;

typedef struct {
    char *value;
    int is_variable_reference;
    int has_array_access;
    ArrayAccess array_access;
} PrintStatement;

#endif
