#ifndef DRIFT_STATEMENT_H
#define DRIFT_STATEMENT_H

#include "drift/ast.h"

struct Statement;

typedef struct {
    char *condition_text;
    struct Statement *body;
    size_t body_count;
} IfBranch;

typedef struct {
    IfBranch *branches;
    size_t branch_count;
    struct Statement *else_body;
    size_t else_count;
} IfStatement;

typedef enum {
    STATEMENT_PRINT,
    STATEMENT_VARIABLE_DECLARATION,
    STATEMENT_INPUT,
    STATEMENT_IF
} StatementType;

typedef struct Statement {
    StatementType type;
    union {
        PrintStatement print_statement;
        VariableDeclaration variable_declaration;
        InputStatement input_statement;
        IfStatement if_statement;
    } as;
} Statement;

#endif
