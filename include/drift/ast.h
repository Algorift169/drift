#ifndef DRIFT_AST_H
#define DRIFT_AST_H

#include "drift/value.h"

typedef struct {
    char *name;
    Value value;
} VariableDeclaration;

typedef struct {
    char *value;
    int is_variable_reference;
} PrintStatement;

#endif
