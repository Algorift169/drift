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

        if (value == 1) {
            return value_create_boolean(1);
        }

        if (value == 0) {
            return value_create_boolean(0);
        }

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

static void append_template_text(char **buffer, size_t *length, size_t *capacity, const char *text)
{
    size_t text_length;

    if (buffer == NULL || length == NULL || capacity == NULL || text == NULL) {
        return;
    }

    text_length = strlen(text);
    if (*length + text_length + 1 > *capacity) {
        size_t new_capacity = *capacity;
        while (*length + text_length + 1 > new_capacity) {
            new_capacity *= 2;
        }
        *buffer = (char *)realloc(*buffer, new_capacity);
        if (*buffer == NULL) {
            fprintf(stderr, "Error: out of memory while building print template\n");
            return;
        }
        *capacity = new_capacity;
    }

    strcat(*buffer, text);
    *length += text_length;
}

Statement parser_parse(Parser *parser)
{
    Token *token;
    Statement statement;
    VariableDeclaration variable_declaration;
    PrintStatement print_statement;
    char *template = NULL;
    size_t template_length = 0;
    size_t template_capacity = 16;

    print_statement.value = NULL;
    print_statement.is_variable_reference = 0;
    print_statement.has_array_access = 0;
    array_access_init(&print_statement.array_access);

    variable_declaration.name = NULL;
    variable_declaration.value = value_create_string(NULL);
    variable_declaration.is_declaration = 0;
    variable_declaration.is_assignment = 0;
    variable_declaration.is_array_expression = 0;
    variable_declaration.is_array_declared = 0;
    array_access_init(&variable_declaration.array_access);

    token = parser_peek(parser);
    if (token == NULL) {
        fprintf(stderr, "Syntax Error: Unexpected end of input.\n");
        statement.type = STATEMENT_PRINT;
        print_statement.value = NULL;
        print_statement.is_variable_reference = 0;
        statement.as.print_statement = print_statement;
        return statement;
    }

    if (token->type == TOKEN_SAY) {
        parser_advance(parser);
        template = (char *)malloc(template_capacity);
        if (template == NULL) {
            fprintf(stderr, "Error: out of memory while building print template\n");
            statement.type = STATEMENT_PRINT;
            print_statement.value = NULL;
            print_statement.is_variable_reference = 0;
            print_statement.has_array_access = 0;
            statement.as.print_statement = print_statement;
            return statement;
        }
        template[0] = '\0';
        print_statement.has_array_access = 0;
        array_access_init(&print_statement.array_access);

        while (1) {
            token = parser_peek(parser);
            if (token == NULL || token->type == TOKEN_NEWLINE || token->type == TOKEN_EOF) {
                break;
            }

            if (token->type == TOKEN_STRING) {
                char *placeholder = NULL;
                if (token->value != NULL && token->value[0] == '\'' && token->value[strlen(token->value) - 1] == '\'') {
                    size_t inner_length = strlen(token->value) - 2U;
                    char *inner = (char *)malloc(inner_length + 1U);
                    if (inner == NULL) {
                        fprintf(stderr, "Error: out of memory while reading single-quoted variable reference\n");
                        free(template);
                        array_access_free(&print_statement.array_access);
                        statement.type = STATEMENT_PRINT;
                        print_statement.value = NULL;
                        print_statement.is_variable_reference = 0;
                        statement.as.print_statement = print_statement;
                        return statement;
                    }
                    memcpy(inner, token->value + 1, inner_length);
                    inner[inner_length] = '\0';
                    placeholder = (char *)malloc(inner_length + 3U);
                    if (placeholder == NULL) {
                        fprintf(stderr, "Error: out of memory while reading single-quoted variable reference\n");
                        free(inner);
                        free(template);
                        array_access_free(&print_statement.array_access);
                        statement.type = STATEMENT_PRINT;
                        print_statement.value = NULL;
                        print_statement.is_variable_reference = 0;
                        statement.as.print_statement = print_statement;
                        return statement;
                    }
                    snprintf(placeholder, inner_length + 3U, "{%s}", inner);
                    free(inner);
                } else {
                    placeholder = (char *)malloc(strlen(token->value) + 1U);
                    if (placeholder == NULL) {
                        fprintf(stderr, "Error: out of memory while reading string\n");
                        free(template);
                        array_access_free(&print_statement.array_access);
                        statement.type = STATEMENT_PRINT;
                        print_statement.value = NULL;
                        print_statement.is_variable_reference = 0;
                        statement.as.print_statement = print_statement;
                        return statement;
                    }
                    strcpy(placeholder, token->value);
                }
                append_template_text(&template, &template_length, &template_capacity, placeholder);
                free(placeholder);
                parser_advance(parser);
                continue;
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
                    if (print_statement.has_array_access) {
                        fprintf(stderr, "Syntax Error: multiple array accesses are not supported in one print statement.\n");
                        free(template);
                        array_access_free(&print_statement.array_access);
                        statement.type = STATEMENT_PRINT;
                        print_statement.value = NULL;
                        print_statement.is_variable_reference = 0;
                        statement.as.print_statement = print_statement;
                        return statement;
                    }

                    if (!parse_array_access(parser, &print_statement.array_access)) {
                        free(template);
                        array_access_free(&print_statement.array_access);
                        statement.type = STATEMENT_PRINT;
                        print_statement.value = NULL;
                        print_statement.is_variable_reference = 0;
                        statement.as.print_statement = print_statement;
                        return statement;
                    }
                    print_statement.has_array_access = 1;
                    continue;
                }

                char *placeholder = NULL;
                size_t placeholder_length = strlen(token->value) + 3;
                placeholder = (char *)malloc(placeholder_length);
                if (placeholder == NULL) {
                    fprintf(stderr, "Error: out of memory while reading identifier\n");
                    free(template);
                    array_access_free(&print_statement.array_access);
                    statement.type = STATEMENT_PRINT;
                    print_statement.value = NULL;
                    print_statement.is_variable_reference = 0;
                    statement.as.print_statement = print_statement;
                    return statement;
                }
                snprintf(placeholder, placeholder_length, "{%s}", token->value);
                append_template_text(&template, &template_length, &template_capacity, placeholder);
                free(placeholder);
                parser_advance(parser);
                continue;
            }

            fprintf(stderr, "Syntax Error: Expected string after 'say'\n");
            free(template);
            array_access_free(&print_statement.array_access);
            statement.type = STATEMENT_PRINT;
            print_statement.value = NULL;
            print_statement.is_variable_reference = 0;
            statement.as.print_statement = print_statement;
            return statement;
        }

        if (template[0] == '\0') {
            if (!print_statement.has_array_access) {
                fprintf(stderr, "Syntax Error: Expected string after 'say'\n");
                free(template);
                statement.type = STATEMENT_PRINT;
                print_statement.value = NULL;
                print_statement.is_variable_reference = 0;
                print_statement.has_array_access = 0;
                statement.as.print_statement = print_statement;
                return statement;
            }

            free(template);
            template = NULL;
        }

        token = parser_peek(parser);
        if (token != NULL && is_statement_terminator(token)) {
            parser_advance(parser);
        }

        if (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF && !is_statement_terminator(parser_peek(parser))) {
            fprintf(stderr, "Syntax Error: unexpected extra tokens.\n");
            free(template);
            statement.type = STATEMENT_PRINT;
            print_statement.value = NULL;
            print_statement.is_variable_reference = 0;
            statement.as.print_statement = print_statement;
            return statement;
        }

        print_statement.value = template;
        print_statement.is_variable_reference = 0;
        statement.type = STATEMENT_PRINT;
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
    }

    if (token->type == TOKEN_IDENTIFIER && parser->index + 1 < parser->count && parser->tokens[parser->index + 1].type == TOKEN_EQUAL) {
        char *name = drift_duplicate_string(token->value);
        parser_advance(parser);
        parser_advance(parser);

        variable_declaration.name = name;
        variable_declaration.is_assignment = 1;
        variable_declaration.is_array_expression = 0;
        variable_declaration.is_array_declared = 0;
        array_access_init(&variable_declaration.array_access);

        token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Expected value after '='.\n");
            variable_declaration.value = value_create_string(NULL);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
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
                if (!parse_array_access(parser, &variable_declaration.array_access)) {
                    free(name);
                    variable_declaration.name = NULL;
                    variable_declaration.value = value_create_string(NULL);
                    statement.type = STATEMENT_VARIABLE_DECLARATION;
                    statement.as.variable_declaration = variable_declaration;
                    return statement;
                }
                variable_declaration.is_array_expression = 0;
                variable_declaration.is_array_declared = 0;
                variable_declaration.value = value_create_string(NULL);
            } else {
                variable_declaration.value = parse_literal_value(token);
                parser_advance(parser);
            }
        } else {
            variable_declaration.value = parse_literal_value(token);
            parser_advance(parser);
        }

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_NEWLINE) {
            parser_advance(parser);
        }

        if (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF) {
            fprintf(stderr, "Syntax Error: unexpected extra tokens.\n");
            free(variable_declaration.name);
            value_free(&variable_declaration.value);
            variable_declaration.name = NULL;
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        statement.type = STATEMENT_VARIABLE_DECLARATION;
        statement.as.variable_declaration = variable_declaration;
        return statement;
    }

    if (token->type == TOKEN_VAR) {
        parser_advance(parser);
        variable_declaration.is_declaration = 1;

        token = parser_peek(parser);
        if (token == NULL || token->type != TOKEN_IDENTIFIER) {
            fprintf(stderr, "Syntax Error: Expected variable identifier after 'var'.\n");
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            variable_declaration.name = NULL;
            variable_declaration.value = value_create_string(NULL);
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        if (!is_identifier_valid(token->value)) {
            fprintf(stderr, "Syntax Error: Invalid identifier '%s'.\n", token->value);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            variable_declaration.name = NULL;
            variable_declaration.value = value_create_string(NULL);
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        variable_declaration.name = drift_duplicate_string(token->value);
        parser_advance(parser);

        variable_declaration.is_assignment = 0;
        variable_declaration.is_array_expression = 0;
        variable_declaration.is_array_declared = 0;
        array_access_init(&variable_declaration.array_access);
        variable_declaration.value = value_create_string(NULL);

        token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Expected '=' or array dimensions after variable name.\n");
            free(variable_declaration.name);
            variable_declaration.name = NULL;
            variable_declaration.value = value_create_string(NULL);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        if (token->type == TOKEN_LEFT_BRACKET) {
            int parse_error = 0;
            variable_declaration.is_array_declared = 1;
            variable_declaration.value = parse_array_declaration(parser, &parse_error);
            if (parse_error) {
                free(variable_declaration.name);
                variable_declaration.name = NULL;
                variable_declaration.value = value_create_string(NULL);
                statement.type = STATEMENT_VARIABLE_DECLARATION;
                statement.as.variable_declaration = variable_declaration;
                return statement;
            }

            token = parser_peek(parser);
            if (token != NULL && token->type == TOKEN_EQUAL) {
                parser_advance(parser);
                variable_declaration.is_assignment = 1;

                token = parser_peek(parser);
                if (token == NULL) {
                    fprintf(stderr, "Syntax Error: Expected value after '='.\n");
                    free(variable_declaration.name);
                    variable_declaration.name = NULL;
                    variable_declaration.value = value_create_string(NULL);
                    statement.type = STATEMENT_VARIABLE_DECLARATION;
                    statement.as.variable_declaration = variable_declaration;
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
                        if (!parse_array_access(parser, &variable_declaration.array_access)) {
                            free(variable_declaration.name);
                            variable_declaration.name = NULL;
                            variable_declaration.value = value_create_string(NULL);
                            statement.type = STATEMENT_VARIABLE_DECLARATION;
                            statement.as.variable_declaration = variable_declaration;
                            return statement;
                        }
                        variable_declaration.is_array_expression = variable_declaration.is_array_declared;
                        variable_declaration.value = value_create_string(NULL);
                    } else {
                        variable_declaration.value = parse_literal_value(token);
                        parser_advance(parser);
                    }
                } else {
                    variable_declaration.value = parse_literal_value(token);
                    parser_advance(parser);
                }
            }
        } else if (token->type == TOKEN_EQUAL) {
            parser_advance(parser);
            variable_declaration.is_assignment = 1;

            token = parser_peek(parser);
            if (token == NULL) {
                fprintf(stderr, "Syntax Error: Expected value after '='.\n");
                free(variable_declaration.name);
                variable_declaration.name = NULL;
                variable_declaration.value = value_create_string(NULL);
                statement.type = STATEMENT_VARIABLE_DECLARATION;
                statement.as.variable_declaration = variable_declaration;
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
                    if (!parse_array_access(parser, &variable_declaration.array_access)) {
                        free(variable_declaration.name);
                        variable_declaration.name = NULL;
                        variable_declaration.value = value_create_string(NULL);
                        statement.type = STATEMENT_VARIABLE_DECLARATION;
                        statement.as.variable_declaration = variable_declaration;
                        return statement;
                    }
                    variable_declaration.is_array_expression = variable_declaration.is_array_declared;
                    variable_declaration.value = value_create_string(NULL);
                } else {
                    variable_declaration.value = parse_literal_value(token);
                    parser_advance(parser);
                }
            } else {
                variable_declaration.value = parse_literal_value(token);
                parser_advance(parser);
            }
        } else {
            fprintf(stderr, "Syntax Error: Expected '=' or array dimensions after variable name.\n");
            free(variable_declaration.name);
            variable_declaration.name = NULL;
            variable_declaration.value = value_create_string(NULL);
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
            return statement;
        }

        token = parser_peek(parser);
        if (token != NULL && is_statement_terminator(token)) {
            parser_advance(parser);
        }

        if (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF && !is_statement_terminator(parser_peek(parser))) {
            fprintf(stderr, "Syntax Error: unexpected extra tokens.\n");
            free(variable_declaration.name);
            value_free(&variable_declaration.value);
            variable_declaration.name = NULL;
            statement.type = STATEMENT_VARIABLE_DECLARATION;
            statement.as.variable_declaration = variable_declaration;
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
        statement.as.print_statement = print_statement;
        return statement;
    }

    fprintf(stderr, "Syntax Error: expected 'say' or 'var'.\n");
    statement.type = STATEMENT_PRINT;
    print_statement.value = NULL;
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

    free(declaration->name);
    declaration->name = NULL;
    value_free(&declaration->value);
    array_access_free(&declaration->array_access);
}
