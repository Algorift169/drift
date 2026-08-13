#ifndef DRIFT_STATEMENT_H
#define DRIFT_STATEMENT_H

#include "drift/ast.h"

typedef enum {
    STATEMENT_PRINT,
    STATEMENT_VARIABLE_DECLARATION,
    STATEMENT_INPUT
} StatementType;

typedef struct {
    StatementType type;
    union {
        PrintStatement print_statement;
        VariableDeclaration variable_declaration;
        InputStatement input_statement;
    } as;
} Statement;

#endif
