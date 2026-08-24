/* AST declarations connect parser-produced nodes with later statement evaluation. */

#ifndef DRIFT_AST_H
#define DRIFT_AST_H

#include "drift/value.h"
#include "drift/array.h"
#include "drift/operator.h"

typedef struct {
    char *name;
    Value value;
    int is_declaration;
    int is_assignment;
    int is_array_element_assignment;
    int is_array_expression;
    int is_array_declared;
    int is_input_expression;
    char *input_prompt;
    char *input_target;
    char *expression_text;
    int has_expression;
    int has_assignment_operator;
    OperatorType assignment_operator;
    ArrayAccess array_access;
} VariableDeclarationSingle;

typedef struct {
    VariableDeclarationSingle *vars;
    size_t count;
} VariableDeclaration;

typedef struct {
    char *value;
    int is_variable_reference;
    int has_array_access;
    char *expression_text;
    int has_expression;
    ArrayAccess array_access;
} PrintStatement;

typedef struct {
    char *prompt;
    char *target_name;
    int has_prompt;
    int has_target;
} InputItem;

typedef struct {
    InputItem *items;
    size_t count;
} InputStatement;

typedef struct {
    char *counter_name;
    int has_range;
    int has_step;
    int is_infinite;
    int is_exclusive_upper;
    int is_exclusive_lower;
    char *start_text;
    char *end_text;
    char *step_text;
    struct Statement *body;
    size_t body_count;
} RepeatStatement;

#endif
