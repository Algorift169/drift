#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/parser.h"

static Token *parser_peek(Parser *parser)
{
    if (parser->index >= parser->count) {
        return NULL;
    }

    return &parser->tokens[parser->index];
}

static Token *parser_advance(Parser *parser)
{
    if (parser->index >= parser->count) {
        return NULL;
    }

    return &parser->tokens[parser->index++];
}

static int is_statement_terminator(Token *token)
{
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

static int is_array_element_assignment(Parser *parser, size_t start)
{
    size_t index = start + 1U;
    int has_index = 0;
    while (index < parser->count && parser->tokens[index].type == TOKEN_LEFT_BRACKET) {
        if (index + 2U >= parser->count ||
            (parser->tokens[index + 1U].type != TOKEN_INTEGER && parser->tokens[index + 1U].type != TOKEN_IDENTIFIER) ||
            parser->tokens[index + 2U].type != TOKEN_RIGHT_BRACKET) {
            return 0;
        }
        has_index = 1;
        index += 3U;
    }
    if (has_index && index < parser->count && parser->tokens[index].type == TOKEN_EQUAL) {
        if (index + 1 < parser->count && parser->tokens[index + 1].type == TOKEN_LEFT_BRACE) {
            return 0;
        }
        return 1;
    }
    return 0;
}

static int is_identifier_valid(const char *name)
{
    size_t i;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    if (!((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z') ||
          name[0] == '_' ||
          name[0] == '-')) {
        return 0;
    }

    for (i = 1; name[i] != '\0'; ++i) {
        if (!((name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9') ||
              name[i] == '_' ||
              name[i] == '-')) {
            return 0;
        }
    }

    return 1;
}

static Value parse_literal_value(Token *token)
{
    if (token == NULL) {
        return value_create_string(NULL);
    }

    if (token->type == TOKEN_INTEGER) {
        char *end = NULL;
        long value = strtol(token->value, &end, 10);
        (void)end;
        return value_create_integer(value);
    }

    if (token->type == TOKEN_FLOAT) {
        char *end = NULL;
        double value = strtod(token->value, &end);
        (void)end;
        return value_create_float(value);
    }

    if (token->type == TOKEN_STRING) {
        if (token->value != NULL && token->value[0] == '\'' && token->value[strlen(token->value) - 1] == '\'') {
            size_t length = strlen(token->value) - 2U;
            char *stripped = (char *)malloc(length + 1U);
            if (stripped != NULL) {
                memcpy(stripped, token->value + 1, length);
                stripped[length] = '\0';
                {
                    Value value = value_create_string(stripped);
                    free(stripped);
                    return value;
                }
            }
        }
        return value_create_string(token->value);
    }

    if (token->type == TOKEN_TRUE) {
        return value_create_boolean(1);
    }

    if (token->type == TOKEN_FALSE) {
        return value_create_boolean(0);
    }

    if (token->type == TOKEN_NULL) {
        return value_create_null();
    }

    if (token->type == TOKEN_INFINITY) {
        return value_create_infinity();
    }

    return value_create_string(NULL);
}

Parser parser_create(Token *tokens, size_t count)
{
    Parser parser;
    parser.tokens = tokens;
    parser.count = count;
    parser.index = 0;
    return parser;
}

static void variable_declaration_single_init(VariableDeclarationSingle *single)
{
    single->name = NULL;
    single->value = value_create_string(NULL);
    single->is_declaration = 0;
    single->is_assignment = 0;
    single->is_array_element_assignment = 0;
    single->is_array_expression = 0;
    single->is_array_declared = 0;
    array_access_init(&single->array_access);
}

static void variable_declaration_single_free(VariableDeclarationSingle *single)
{
    if (single == NULL) {
        return;
    }
    free(single->name);
    single->name = NULL;
    value_free(&single->value);
    array_access_free(&single->array_access);
}

static int parse_single_declaration(Parser *parser, VariableDeclarationSingle *single, int is_declaration)
{
    Token *token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER) {
        if (is_declaration) {
            fprintf(stderr, "Syntax Error: Expected variable identifier after 'var'.\n");
        } else {
            fprintf(stderr, "Syntax Error: Expected variable identifier.\n");
        }
        return 0;
    }

    if (!is_identifier_valid(token->value)) {
        fprintf(stderr, "Syntax Error: Invalid identifier '%s'.\n", token->value);
        return 0;
    }

    single->name = drift_duplicate_string(token->value);
    parser_advance(parser);

    single->is_declaration = is_declaration;
    single->is_assignment = 0;
    single->is_array_element_assignment = 0;
    single->is_array_expression = 0;
    single->is_array_declared = 0;
    array_access_init(&single->array_access);
    single->value = value_create_string(NULL);

    token = parser_peek(parser);
    if (token == NULL || is_statement_terminator(token) || token->type == TOKEN_COMMA) {
        fprintf(stderr, "Syntax Error: Expected '=' or array dimensions after variable name.\n");
        return 0;
    }

    if (is_array_element_assignment(parser, parser->index - 1U)) {
        single->is_declaration = 0;
        single->is_assignment = 1;
        single->is_array_element_assignment = 1;
        parser->index--;
        if (!parse_array_access(parser, &single->array_access) ||
            parser_peek(parser) == NULL || parser_peek(parser)->type != TOKEN_EQUAL) {
            fprintf(stderr, "Syntax Error: Expected '=' after array access.\n");
            return 0;
        }
        parser_advance(parser);
        token = parser_peek(parser);
        if (token == NULL || is_statement_terminator(token) || token->type == TOKEN_COMMA) {
            fprintf(stderr, "Syntax Error: Expected literal value after '='.\n");
            return 0;
        }
        single->value = parse_literal_value(token);
        if (single->value.type == VALUE_STRING && single->value.string_value == NULL) {
            fprintf(stderr, "Syntax Error: Expected literal value after '='.\n");
            return 0;
        }
        parser_advance(parser);
        return 1;
    }

    if (token->type == TOKEN_LEFT_BRACKET) {
        int parse_error = 0;
        single->is_array_declared = 1;
        single->value = parse_array_declaration(parser, &parse_error);
        if (parse_error) {
            return 0;
        }

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_EQUAL) {
            parser_advance(parser);
            single->is_assignment = 1;

            token = parser_peek(parser);
            if (token == NULL || is_statement_terminator(token) || token->type == TOKEN_COMMA) {
                fprintf(stderr, "Syntax Error: Expected value after '='.\n");
                return 0;
            }

            if (token->type == TOKEN_IDENTIFIER) {
                Token *next_token = NULL;
                if (parser->index + 1 < parser->count) {
                    next_token = &parser->tokens[parser->index + 1];
                }

                if (next_token != NULL && (next_token->type == TOKEN_LEFT_BRACKET ||
                                           (next_token->type == TOKEN_DOT && parser->index + 2 < parser->count &&
                                            parser->tokens[parser->index + 2].type == TOKEN_IDENTIFIER &&
                                            strcmp(parser->tokens[parser->index + 2].value, "select") == 0))) {
                    if (!parse_array_access(parser, &single->array_access)) {
                        return 0;
                    }
                    single->is_array_expression = single->is_array_declared;
                    single->value = value_create_string(NULL);
                } else {
                    single->value = parse_literal_value(token);
                    parser_advance(parser);
                }
            } else {
                single->value = parse_literal_value(token);
                parser_advance(parser);
            }
        }
    } else if (token->type == TOKEN_EQUAL) {
        parser_advance(parser);
        single->is_assignment = 1;

        token = parser_peek(parser);
        if (token == NULL || is_statement_terminator(token) || token->type == TOKEN_COMMA) {
            fprintf(stderr, "Syntax Error: Expected value after '='.\n");
            return 0;
        }

        if (token->type == TOKEN_IDENTIFIER) {
            Token *next_token = NULL;
            if (parser->index + 1 < parser->count) {
                next_token = &parser->tokens[parser->index + 1];
            }

            if (next_token != NULL && (next_token->type == TOKEN_LEFT_BRACKET ||
                                       (next_token->type == TOKEN_DOT && parser->index + 2 < parser->count &&
                                        parser->tokens[parser->index + 2].type == TOKEN_IDENTIFIER &&
                                        strcmp(parser->tokens[parser->index + 2].value, "select") == 0))) {
                if (!parse_array_access(parser, &single->array_access)) {
                    return 0;
                }
                single->is_array_expression = single->is_array_declared;
                single->value = value_create_string(NULL);
            } else {
                single->value = parse_literal_value(token);
                parser_advance(parser);
            }
        } else {
            single->value = parse_literal_value(token);
            parser_advance(parser);
        }
    } else {
        fprintf(stderr, "Syntax Error: Expected '=' or array dimensions after variable name.\n");
        return 0;
    }

    return 1;
}

Statement parser_parse(Parser *parser)
{
    Token *token;
    Statement statement;
    PrintStatement print_statement;
    VariableDeclaration variable_declaration;
    variable_declaration.vars = NULL;
    variable_declaration.count = 0;

    token = parser_peek(parser);
    while (token != NULL && token->type == TOKEN_NEWLINE) {
        parser_advance(parser);
        token = parser_peek(parser);
    }

    if (token == NULL || token->type == TOKEN_EOF) {
        statement.type = STATEMENT_PRINT;
        print_statement.value = NULL;
        print_statement.is_variable_reference = 0;
        print_statement.has_array_access = 0;
        array_access_init(&print_statement.array_access);
        statement.as.print_statement = print_statement;
        return statement;
    }

    if (token->type == TOKEN_SAY) {
        parser_advance(parser);
        token = parser_peek(parser);

        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Expected string literal or variable reference after 'say'.\n");
            statement.type = STATEMENT_PRINT;
            print_statement.value = NULL;
            print_statement.is_variable_reference = 0;
            print_statement.has_array_access = 0;
            array_access_init(&print_statement.array_access);
            statement.as.print_statement = print_statement;
            return statement;
        }

        if (token->type == TOKEN_IDENTIFIER) {
            Token *next_token = NULL;
            if (parser->index + 1 < parser->count) {
                next_token = &parser->tokens[parser->index + 1];
            }

            if (next_token != NULL && (next_token->type == TOKEN_LEFT_BRACKET ||
                                       (next_token->type == TOKEN_DOT && parser->index + 2 < parser->count &&
                                        parser->tokens[parser->index + 2].type == TOKEN_IDENTIFIER &&
                                        strcmp(parser->tokens[parser->index + 2].value, "select") == 0))) {
                if (!parse_array_access(parser, &print_statement.array_access)) {
                    statement.type = STATEMENT_PRINT;
                    print_statement.value = NULL;
                    print_statement.is_variable_reference = 0;
                    print_statement.has_array_access = 0;
                    array_access_init(&print_statement.array_access);
                    statement.as.print_statement = print_statement;
                    return statement;
                }

                print_statement.has_array_access = 1;
                print_statement.value = NULL;
                print_statement.is_variable_reference = 0;
                statement.type = STATEMENT_PRINT;
                statement.as.print_statement = print_statement;
                return statement;
            }

            print_statement.is_variable_reference = 1;
            print_statement.has_array_access = 0;
            array_access_init(&print_statement.array_access);
            print_statement.value = drift_duplicate_string(token->value);
            parser_advance(parser);
            statement.type = STATEMENT_PRINT;
            statement.as.print_statement = print_statement;

            token = parser_peek(parser);
            if (token != NULL && is_statement_terminator(token)) {
                parser_advance(parser);
                return statement;
            }

            if (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF) {
                fprintf(stderr, "Syntax Error: unexpected extra tokens.\n");
                free(print_statement.value);
                print_statement.value = NULL;
                statement.as.print_statement = print_statement;
                return statement;
            }

            return statement;
        }

        if (token->type == TOKEN_STRING) {
            print_statement.is_variable_reference = 0;
            print_statement.has_array_access = 0;
            array_access_init(&print_statement.array_access);

            if (token->value != NULL && token->value[0] == '\'' && token->value[strlen(token->value) - 1] == '\'') {
                size_t length = strlen(token->value) - 2U;
                char *stripped = (char *)malloc(length + 1U);
                if (stripped != NULL) {
                    memcpy(stripped, token->value + 1, length);
                    stripped[length] = '\0';
                    print_statement.value = stripped;
                } else {
                    print_statement.value = drift_duplicate_string(token->value);
                }
            } else {
                print_statement.value = drift_duplicate_string(token->value);
            }

            parser_advance(parser);
            statement.type = STATEMENT_PRINT;
            statement.as.print_statement = print_statement;

            token = parser_peek(parser);
            if (token != NULL && is_statement_terminator(token)) {
                parser_advance(parser);
                return statement;
            }

            if (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF) {
                fprintf(stderr, "Syntax Error: unexpected extra tokens.\n");
                free(print_statement.value);
                print_statement.value = NULL;
                statement.as.print_statement = print_statement;
                return statement;
            }

            return statement;
        }

        fprintf(stderr, "Syntax Error: Expected string literal or variable reference after 'say'.\n");
        statement.type = STATEMENT_PRINT;
        print_statement.value = NULL;
        print_statement.is_variable_reference = 0;
        print_statement.has_array_access = 0;
        array_access_init(&print_statement.array_access);
        statement.as.print_statement = print_statement;
        return statement;
    }

    if (token->type == TOKEN_IDENTIFIER && is_array_element_assignment(parser, parser->index)) {
        VariableDeclarationSingle single;
        variable_declaration_single_init(&single);
        single.name = drift_duplicate_string(token->value);
        single.is_assignment = 1;
        single.is_array_element_assignment = 1;
        if (!parse_array_access(parser, &single.array_access)) {
            variable_declaration_single_free(&single);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }
        if (parser_peek(parser) == NULL || parser_peek(parser)->type != TOKEN_EQUAL) {
            fprintf(stderr, "Syntax Error: Expected '=' after array access.\n");
            variable_declaration_single_free(&single);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }
        parser_advance(parser);
        token = parser_peek(parser);
        single.value = parse_literal_value(token);
        if (token == NULL || (single.value.type == VALUE_STRING && single.value.string_value == NULL)) {
            fprintf(stderr, "Syntax Error: Expected literal value after '='.\n");
            variable_declaration_single_free(&single);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        } else {
            parser_advance(parser);
        }

        variable_declaration.vars = (VariableDeclarationSingle *)malloc(sizeof(VariableDeclarationSingle));
        if (variable_declaration.vars != NULL) {
            variable_declaration.vars[0] = single;
            variable_declaration.count = 1;
        } else {
            variable_declaration_single_free(&single);
        }
        statement.type = STATEMENT_VARIABLE_DECLARATION;
        statement.as.variable_declaration = variable_declaration;
        return statement;
    }

    if (token->type == TOKEN_IDENTIFIER && parser->index + 1 < parser->count && parser->tokens[parser->index + 1].type == TOKEN_EQUAL) {
        VariableDeclarationSingle single;
        variable_declaration_single_init(&single);
        if (!parse_single_declaration(parser, &single, 0)) {
            variable_declaration_single_free(&single);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        variable_declaration.vars = (VariableDeclarationSingle *)malloc(sizeof(VariableDeclarationSingle));
        if (variable_declaration.vars != NULL) {
            variable_declaration.vars[0] = single;
            variable_declaration.count = 1;
        } else {
            variable_declaration_single_free(&single);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        token = parser_peek(parser);
        if (token != NULL && is_statement_terminator(token)) {
            parser_advance(parser);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        if (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF) {
            fprintf(stderr, "Syntax Error: unexpected extra tokens.\n");
            variable_declaration_free(&variable_declaration);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration.vars = NULL;
            statement.as.variable_declaration.count = 0;
            return statement;
        }

        statement.type = STATEMENT_VARIABLE_DECLARATION;
        statement.as.variable_declaration = variable_declaration;
        return statement;
    }

    if (token->type == TOKEN_VAR) {
        parser_advance(parser);
        variable_declaration.vars = NULL;
        variable_declaration.count = 0;

        while (1) {
            VariableDeclarationSingle single;
            variable_declaration_single_init(&single);

            if (!parse_single_declaration(parser, &single, 1)) {
                variable_declaration_single_free(&single);
                variable_declaration_free(&variable_declaration);
                statement.type = STATEMENT_VARIABLE_DECLARATION;
                statement.as.variable_declaration.vars = NULL;
                statement.as.variable_declaration.count = 0;
                return statement;
            }

            VariableDeclarationSingle *new_vars = (VariableDeclarationSingle *)realloc(
                variable_declaration.vars,
                (variable_declaration.count + 1U) * sizeof(VariableDeclarationSingle)
            );
            if (new_vars == NULL) {
                fprintf(stderr, "Error: out of memory while parsing variable declaration\n");
                variable_declaration_single_free(&single);
                variable_declaration_free(&variable_declaration);
                statement.type = STATEMENT_VARIABLE_DECLARATION;
                statement.as.variable_declaration.vars = NULL;
                statement.as.variable_declaration.count = 0;
                return statement;
            }
            variable_declaration.vars = new_vars;
            variable_declaration.vars[variable_declaration.count++] = single;

            token = parser_peek(parser);
            if (token != NULL && token->type == TOKEN_COMMA) {
                parser_advance(parser);
                continue;
            }
            break;
        }

        token = parser_peek(parser);
        if (token != NULL && is_statement_terminator(token)) {
            parser_advance(parser);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        if (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF) {
            fprintf(stderr, "Syntax Error: unexpected extra tokens.\n");
            variable_declaration_free(&variable_declaration);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration.vars = NULL;
            statement.as.variable_declaration.count = 0;
            return statement;
        }

        statement.type = STATEMENT_VARIABLE_DECLARATION;
        statement.as.variable_declaration = variable_declaration;
        return statement;
    }

    if (token->type == TOKEN_IDENTIFIER) {
        fprintf(stderr, "Syntax Error: Only literal values are allowed during variable declaration in Drift v0.2.\n");
        statement.type = STATEMENT_PRINT;
        print_statement.value = NULL;
        print_statement.is_variable_reference = 0;
        print_statement.has_array_access = 0;
        array_access_init(&print_statement.array_access);
        statement.as.print_statement = print_statement;
        return statement;
    }

    fprintf(stderr, "Syntax Error: expected 'say' or 'var'.\n");
    statement.type = STATEMENT_PRINT;
    print_statement.value = NULL;
    print_statement.is_variable_reference = 0;
    print_statement.has_array_access = 0;
    array_access_init(&print_statement.array_access);
    statement.as.print_statement = print_statement;
    return statement;
}

void print_statement_free(PrintStatement *statement)
{
    if (statement == NULL) {
        return;
    }

    free(statement->value);
    statement->value = NULL;
    array_access_free(&statement->array_access);
}

void variable_declaration_free(VariableDeclaration *declaration)
{
    if (declaration == NULL) {
        return;
    }

    if (declaration->vars != NULL) {
        for (size_t i = 0; i < declaration->count; ++i) {
            variable_declaration_single_free(&declaration->vars[i]);
        }
        free(declaration->vars);
        declaration->vars = NULL;
    }
    declaration->count = 0;
}
