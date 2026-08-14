#ifndef DRIFT_PARSER_H
#define DRIFT_PARSER_H

#include <stddef.h>

#include "drift/ast.h"
#include "drift/statement.h"
#include "drift/token.h"

typedef struct Parser {
    Token *tokens;
    size_t count;
    size_t index;
} Parser;

Parser parser_create(Token *tokens, size_t count);
Statement parser_parse(Parser *parser);
int parse_if_statement(Parser *parser, Statement *statement);
void if_statement_free(IfStatement *statement);
void print_statement_free(PrintStatement *statement);
void variable_declaration_free(VariableDeclaration *declaration);

#endif
