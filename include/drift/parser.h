/* Parser declarations describe cursor movement 
and the invariant that input is consumed once. */

#ifndef DRIFT_PARSER_H
#define DRIFT_PARSER_H

#include <stddef.h>

#include "drift/ast.h"
#include "drift/statement.h"
#include "drift/token.h"

// Parser structure to hold the state of the parser.
typedef struct Parser {
    Token *tokens;
    size_t count;
    size_t index;
} Parser;
 
/*
 The parser_create function initializes a Parser structure with the given 
  tokens and count. It sets the index to 0, indicating that parsing will
    start from the beginning of the token array.
*/
Parser parser_create(Token *tokens, size_t count);
Statement parser_parse(Parser *parser);
int parse_if_statement(Parser *parser, Statement *statement);
int parse_repeat_statement(Parser *parser, Statement *statement);
void if_statement_free(IfStatement *statement);
void repeat_statement_free(RepeatStatement *statement);
void print_statement_free(PrintStatement *statement);
void variable_declaration_free(VariableDeclaration *declaration);

#endif
