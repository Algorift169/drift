#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/array_value.h"
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

static int parser_expect(Parser *parser, TokenType type, const char *message)
{
    Token *token = parser_peek(parser);
    if (token == NULL || token->type != type) {
        if (message != NULL) {
            fprintf(stderr, "%s\n", message);
        }
        return 0;
    }
    parser_advance(parser);
    return 1;
}

static int is_statement_terminator(Token *token)
{
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

static int is_assignment_operator_token(TokenType type)
{
    return type == TOKEN_EQUAL || type == TOKEN_PLUS_EQUAL || type == TOKEN_MINUS_EQUAL ||
           type == TOKEN_STAR_EQUAL || type == TOKEN_SLASH_EQUAL || type == TOKEN_PERCENT_EQUAL ||
           type == TOKEN_AMPERSAND_EQUAL || type == TOKEN_PIPE_EQUAL || type == TOKEN_CARET_EQUAL ||
           type == TOKEN_SHIFT_LEFT_EQUAL || type == TOKEN_SHIFT_RIGHT_EQUAL || type == TOKEN_PLUS_PLUS ||
           type == TOKEN_MINUS_MINUS;
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

static int parse_ask_statement(Parser *parser, Statement *statement);

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

static Value parse_array_literal_value(Parser *parser, int *error)
{
    Value result = value_create_null();
    Value *values = NULL;
    size_t value_count = 0;
    ValueType element_type = VALUE_NULL;

    if (parser == NULL || error == NULL) {
        return result;
    }

    *error = 0;
    if (parser_peek(parser) == NULL || parser_peek(parser)->type != TOKEN_LEFT_BRACKET) {
        *error = 1;
        return result;
    }
    parser_advance(parser);

    while (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_RIGHT_BRACKET) {
        Token *token = parser_peek(parser);
        Value item = value_create_null();

        if (token == NULL) {
            *error = 1;
            break;
        }

        if (token->type == TOKEN_LEFT_BRACKET) {
            int nested_error = 0;
            item = parse_array_literal_value(parser, &nested_error);
            if (nested_error) {
                *error = 1;
                break;
            }
        } else if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_INTEGER || token->type == TOKEN_FLOAT ||
                   token->type == TOKEN_STRING || token->type == TOKEN_TRUE || token->type == TOKEN_FALSE ||
                   token->type == TOKEN_NULL || token->type == TOKEN_INFINITY) {
            item = parse_literal_value(token);
            parser_advance(parser);
        } else {
            fprintf(stderr, "Syntax Error: Invalid array literal element.\n");
            *error = 1;
            break;
        }

        if (value_count == 0) {
            element_type = item.type;
        }

        Value *new_values = (Value *)realloc(values, (value_count + 1U) * sizeof(Value));
        if (new_values == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array literal.\n");
            value_free(&item);
            *error = 1;
            break;
        }
        values = new_values;
        values[value_count++] = item;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
        }
    }

    if (*error == 0 && !parser_expect(parser, TOKEN_RIGHT_BRACKET, "Syntax Error: Expected ']' to close array literal.")) {
        *error = 1;
    }

    if (*error != 0) {
        for (size_t i = 0; i < value_count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        return value_create_null();
    }

    if (value_count == 0) {
        ArrayValue *array = array_value_create_dynamic_from_values(VALUE_NULL, NULL, 0);
        result = value_create_array(array);
        free(values);
        return result;
    }

    ArrayValue *array = array_value_create_dynamic_from_values(element_type, values, value_count);
    for (size_t i = 0; i < value_count; ++i) {
        value_free(&values[i]);
    }
    free(values);
    result = value_create_array(array);
    return result;
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
    single->value = value_create_null();
    single->is_declaration = 0;
    single->is_assignment = 0;
    single->is_array_element_assignment = 0;
    single->is_array_expression = 0;
    single->is_array_declared = 0;
    single->is_input_expression = 0;
    single->input_prompt = NULL;
    single->input_target = NULL;
    single->expression_text = NULL;
    single->has_expression = 0;
    single->has_assignment_operator = 0;
    single->assignment_operator = OPERATOR_NONE;
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
    free(single->input_prompt);
    free(single->input_target);
    free(single->expression_text);
    single->input_prompt = NULL;
    single->input_target = NULL;
    single->expression_text = NULL;
    single->has_expression = 0;
    single->has_assignment_operator = 0;
    single->assignment_operator = OPERATOR_NONE;
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
        single->value = value_create_null();
        return 1;
    }

    if (token->type == TOKEN_PLUS_EQUAL || token->type == TOKEN_MINUS_EQUAL || token->type == TOKEN_STAR_EQUAL ||
        token->type == TOKEN_SLASH_EQUAL || token->type == TOKEN_PERCENT_EQUAL || token->type == TOKEN_AMPERSAND_EQUAL ||
        token->type == TOKEN_PIPE_EQUAL || token->type == TOKEN_CARET_EQUAL || token->type == TOKEN_SHIFT_LEFT_EQUAL ||
        token->type == TOKEN_SHIFT_RIGHT_EQUAL || token->type == TOKEN_PLUS_PLUS || token->type == TOKEN_MINUS_MINUS) {
        single->is_assignment = 1;
        single->has_assignment_operator = 1;
        if (token->type == TOKEN_PLUS_EQUAL) {
            single->assignment_operator = OPERATOR_ADD_ASSIGN;
        } else if (token->type == TOKEN_MINUS_EQUAL) {
            single->assignment_operator = OPERATOR_SUBTRACT_ASSIGN;
        } else if (token->type == TOKEN_STAR_EQUAL) {
            single->assignment_operator = OPERATOR_MULTIPLY_ASSIGN;
        } else if (token->type == TOKEN_SLASH_EQUAL) {
            single->assignment_operator = OPERATOR_DIVIDE_ASSIGN;
        } else if (token->type == TOKEN_PERCENT_EQUAL) {
            single->assignment_operator = OPERATOR_MODULO_ASSIGN;
        } else if (token->type == TOKEN_AMPERSAND_EQUAL) {
            single->assignment_operator = OPERATOR_AND_ASSIGN;
        } else if (token->type == TOKEN_PIPE_EQUAL) {
            single->assignment_operator = OPERATOR_OR_ASSIGN;
        } else if (token->type == TOKEN_CARET_EQUAL) {
            single->assignment_operator = OPERATOR_XOR_ASSIGN;
        } else if (token->type == TOKEN_SHIFT_LEFT_EQUAL) {
            single->assignment_operator = OPERATOR_SHIFT_LEFT_ASSIGN;
        } else if (token->type == TOKEN_SHIFT_RIGHT_EQUAL) {
            single->assignment_operator = OPERATOR_SHIFT_RIGHT_ASSIGN;
        } else if (token->type == TOKEN_PLUS_PLUS) {
            single->assignment_operator = OPERATOR_INCREMENT;
        } else if (token->type == TOKEN_MINUS_MINUS) {
            single->assignment_operator = OPERATOR_DECREMENT;
        }

        parser_advance(parser);
        if (token->type == TOKEN_PLUS_PLUS || token->type == TOKEN_MINUS_MINUS) {
            single->value = value_create_null();
            return 1;
        }

        size_t length = 0;
        Token *current = parser_peek(parser);
        while (current != NULL && !is_statement_terminator(current) && current->type != TOKEN_COMMA && current->type != TOKEN_EOF) {
            length += strlen(current->value ? current->value : "") + 1U;
            current = &parser->tokens[parser->index + (size_t)(current - &parser->tokens[parser->index]) + 1U];
        }

        single->has_expression = 1;
        single->expression_text = (char *)malloc(length + 1U);
        if (single->expression_text == NULL) {
            fprintf(stderr, "Error: out of memory while reading compound assignment expression.\n");
            return 0;
        }
        single->expression_text[0] = '\0';

        while (parser->index < parser->count && !is_statement_terminator(&parser->tokens[parser->index]) && parser->tokens[parser->index].type != TOKEN_COMMA && parser->tokens[parser->index].type != TOKEN_EOF) {
            Token *tok = &parser->tokens[parser->index];
            if (single->expression_text[0] != '\0') {
                strcat(single->expression_text, " ");
            }
            if (tok->value != NULL) {
                strcat(single->expression_text, tok->value);
            }
            parser_advance(parser);
        }
        single->value = value_create_null();
        return 1;
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
        if (parser->index + 1 < parser->count && parser->tokens[parser->index + 1].type != TOKEN_INTEGER &&
            parser->tokens[parser->index + 1].type != TOKEN_RIGHT_BRACKET) {
            int parse_error = 0;
            single->value = parse_array_literal_value(parser, &parse_error);
            if (parse_error) {
                return 0;
            }
            single->is_assignment = 1;
            return 1;
        }

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

        if (token->type == TOKEN_ASK) {
            Statement input_statement;
            if (!parse_ask_statement(parser, &input_statement)) {
                return 0;
            }

            single->is_input_expression = 1;
            if (input_statement.as.input_statement.count > 0) {
                single->input_prompt = input_statement.as.input_statement.items[0].prompt;
                single->input_target = input_statement.as.input_statement.items[0].target_name;
            } else {
                single->input_prompt = NULL;
                single->input_target = NULL;
            }
            single->value = value_create_null();
            if (input_statement.as.input_statement.items != NULL) {
                for (size_t i = 0; i < input_statement.as.input_statement.count; ++i) {
                    input_statement.as.input_statement.items[i].prompt = NULL;
                    input_statement.as.input_statement.items[i].target_name = NULL;
                }
            }
        } else if (token->type == TOKEN_LEFT_BRACKET) {
            int array_literal_error = 0;
            single->value = parse_array_literal_value(parser, &array_literal_error);
            if (array_literal_error) {
                return 0;
            }
            single->is_assignment = 1;
            return 1;
        } else {
            size_t start_index = parser->index;
            size_t length = 0;
            Token *current = parser_peek(parser);
            while (current != NULL && !is_statement_terminator(current) && current->type != TOKEN_COMMA && current->type != TOKEN_EOF) {
                length += strlen(current->value ? current->value : "") + 1U;
                current = &parser->tokens[parser->index + (size_t)(current - &parser->tokens[start_index]) + 1U];
            }

            single->has_expression = 1;
            single->expression_text = (char *)malloc(length + 1U);
            if (single->expression_text == NULL) {
                fprintf(stderr, "Error: out of memory while reading expression.\n");
                return 0;
            }
            single->expression_text[0] = '\0';

            while (parser->index < parser->count && !is_statement_terminator(&parser->tokens[parser->index]) && parser->tokens[parser->index].type != TOKEN_COMMA && parser->tokens[parser->index].type != TOKEN_EOF) {
                Token *tok = &parser->tokens[parser->index];
                if (single->expression_text[0] != '\0') {
                    strcat(single->expression_text, " ");
                }
                if (tok->value != NULL) {
                    strcat(single->expression_text, tok->value);
                }
                parser_advance(parser);
            }
            single->value = value_create_null();
        }
    } else {
        fprintf(stderr, "Syntax Error: Expected '=' or array dimensions after variable name.\n");
        return 0;
    }

    return 1;
}

static char *parse_prompt_or_string(Parser *parser)
{
    Token *token = parser_peek(parser);
    char *result = NULL;

    if (token == NULL) {
        return NULL;
    }

    if (token->type == TOKEN_STRING) {
        if (token->value != NULL && token->value[0] == '\'' && token->value[strlen(token->value) - 1] == '\'') {
            size_t length = strlen(token->value) - 2U;
            char *stripped = (char *)malloc(length + 1U);
            if (stripped != NULL) {
                memcpy(stripped, token->value + 1, length);
                stripped[length] = '\0';
                result = stripped;
            } else {
                result = drift_duplicate_string(token->value);
            }
        } else {
            result = drift_duplicate_string(token->value);
        }
        parser_advance(parser);
        return result;
    }

    return NULL;
}

static char **extract_implicit_targets_from_prompt(char **prompt, size_t *out_count)
{
    char *text;
    char *open;
    char *close;
    char *cleaned;
    char **targets = NULL;
    size_t count = 0;
    size_t capacity = 4;

    if (out_count != NULL) {
        *out_count = 0;
    }

    if (prompt == NULL || *prompt == NULL) {
        return NULL;
    }

    text = *prompt;
    open = strchr(text, '{');
    if (open == NULL) {
        return NULL;
    }

    close = strchr(open + 1, '}');
    if (close == NULL) {
        return NULL;
    }

    targets = (char **)malloc(capacity * sizeof(char *));
    if (targets == NULL) {
        return NULL;
    }

    const char *p = open + 1;
    while (p < close) {
        while (p < close && (isspace((unsigned char)*p) || *p == '@' || *p == ',')) {
            p++;
        }
        if (p >= close) break;

        const char *start = p;
        while (p < close && (isalnum((unsigned char)*p) || *p == '_')) {
            p++;
        }

        size_t len = (size_t)(p - start);
        if (len > 0) {
            char *name = (char *)malloc(len + 1U);
            if (name != NULL) {
                memcpy(name, start, len);
                name[len] = '\0';

                if (count >= capacity) {
                    capacity *= 2U;
                    char **new_targets = (char **)realloc(targets, capacity * sizeof(char *));
                    if (new_targets == NULL) {
                        free(name);
                        break;
                    }
                    targets = new_targets;
                }
                targets[count++] = name;
            }
        } else {
            p++;
        }
    }

    cleaned = (char *)malloc(strlen(text) - (close - open) + 1U);
    if (cleaned != NULL) {
        memcpy(cleaned, text, (size_t)(open - text));
        memcpy(cleaned + (open - text), close + 1, strlen(close + 1) + 1U);
        free(*prompt);
        *prompt = cleaned;
    }

    if (out_count != NULL) {
        *out_count = count;
    }

    if (count == 0) {
        free(targets);
        return NULL;
    }

    return targets;
}

static int parse_ask_statement(Parser *parser, Statement *statement)
{
    InputStatement input_statement;
    size_t item_count = 0;
    size_t item_capacity = 4;
    InputItem *items = (InputItem *)calloc(item_capacity, sizeof(InputItem));
    Token *token;

    if (items == NULL) {
        fprintf(stderr, "Error: out of memory while parsing ask statement.\n");
        return 0;
    }

    input_statement.items = items;
    input_statement.count = 0;

    parser_advance(parser);

    while (1) {
        InputItem item;
        char *prompt = NULL;
        char *target_name = NULL;
        int saw_prompt = 0;
        int saw_target = 0;

        item.prompt = NULL;
        item.target_name = NULL;
        item.has_prompt = 0;
        item.has_target = 0;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_STRING) {
            prompt = parse_prompt_or_string(parser);
            item.prompt = prompt;
            item.has_prompt = 1;
            saw_prompt = 1;
            token = parser_peek(parser);
        }

        if (prompt != NULL) {
            size_t target_count = 0;
            char **implicit_targets = extract_implicit_targets_from_prompt(&prompt, &target_count);
            if (implicit_targets != NULL && target_count > 0) {
                item.prompt = NULL;
                item.target_name = NULL;
                item.has_prompt = 0;
                item.has_target = 0;

                for (size_t k = 0; k < target_count; ++k) {
                    InputItem sub_item;
                    sub_item.prompt = (k == 0) ? prompt : NULL;
                    sub_item.target_name = implicit_targets[k];
                    sub_item.has_prompt = (k == 0) ? 1 : 0;
                    sub_item.has_target = 1;

                    if (item_count >= item_capacity) {
                        size_t new_capacity = item_capacity * 2U;
                        InputItem *new_items = (InputItem *)realloc(items, new_capacity * sizeof(InputItem));
                        if (new_items == NULL) {
                            fprintf(stderr, "Error: out of memory while parsing ask statement.\n");
                            free(implicit_targets);
                            return 0;
                        }
                        items = new_items;
                        item_capacity = new_capacity;
                    }
                    items[item_count++] = sub_item;
                }
                free(implicit_targets);
                saw_prompt = 1;
                saw_target = 1;
                prompt = NULL;
            } else {
                item.prompt = prompt;
                item.has_prompt = 1;
                saw_prompt = 1;
            }
        }

        if (token != NULL && token->type == TOKEN_AT) {
            parser_advance(parser);
            token = parser_peek(parser);
            if (token == NULL || token->type != TOKEN_IDENTIFIER) {
                fprintf(stderr, "Syntax Error: Expected variable name after '@'.\n");
                free(prompt);
                free(target_name);
                for (size_t i = 0; i < item_count; ++i) {
                    free(items[i].prompt);
                    free(items[i].target_name);
                }
                free(items);
                return 0;
            }
            target_name = drift_duplicate_string(token->value);
            parser_advance(parser);
            item.target_name = target_name;
            item.has_target = 1;
            saw_target = 1;
        }

        if (!saw_prompt && !saw_target) {
            fprintf(stderr, "Syntax Error: ask requires a prompt or a target variable.\n");
            free(prompt);
            free(target_name);
            for (size_t i = 0; i < item_count; ++i) {
                free(items[i].prompt);
                free(items[i].target_name);
            }
            free(items);
            return 0;
        }

        if (item.has_prompt || item.has_target) {
            if (item_count >= item_capacity) {
                size_t new_capacity = item_capacity * 2U;
                InputItem *new_items = (InputItem *)realloc(items, new_capacity * sizeof(InputItem));
                if (new_items == NULL) {
                    fprintf(stderr, "Error: out of memory while parsing ask statement.\n");
                    for (size_t i = 0; i < item_count; ++i) {
                        free(items[i].prompt);
                        free(items[i].target_name);
                    }
                    free(items);
                    return 0;
                }
                items = new_items;
                item_capacity = new_capacity;
            }

            items[item_count++] = item;
        }

        token = parser_peek(parser);
        if (token == NULL || token->type == TOKEN_EOF || is_statement_terminator(token)) {
            break;
        }

        if (token->type != TOKEN_STRING && token->type != TOKEN_AT) {
            break;
        }
    }

    input_statement.items = items;
    input_statement.count = item_count;
    statement->type = STATEMENT_INPUT;
    statement->as.input_statement = input_statement;
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
            fprintf(stderr, "Syntax Error: Expected expression, string literal or variable reference after 'say'.\n");
            statement.type = STATEMENT_PRINT;
            print_statement.value = NULL;
            print_statement.is_variable_reference = 0;
            print_statement.has_array_access = 0;
            print_statement.has_expression = 0;
            print_statement.expression_text = NULL;
            array_access_init(&print_statement.array_access);
            statement.as.print_statement = print_statement;
            return statement;
        }

        Token *next_token = (parser->index + 1 < parser->count) ? &parser->tokens[parser->index + 1] : NULL;

        if (token->type == TOKEN_STRING && (next_token == NULL || is_statement_terminator(next_token) || next_token->type == TOKEN_EOF)) {
            print_statement.is_variable_reference = 0;
            print_statement.has_array_access = 0;
            print_statement.has_expression = 0;
            print_statement.expression_text = NULL;
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
            }
            return statement;
        }

        if (token->type == TOKEN_IDENTIFIER) {
            if (next_token != NULL && (next_token->type == TOKEN_LEFT_BRACKET ||
                                       (next_token->type == TOKEN_DOT && parser->index + 2 < parser->count &&
                                        parser->tokens[parser->index + 2].type == TOKEN_IDENTIFIER &&
                                        strcmp(parser->tokens[parser->index + 2].value, "select") == 0))) {
                if (parse_array_access(parser, &print_statement.array_access)) {
                    Token *after_access = parser_peek(parser);
                    if (after_access != NULL && (is_statement_terminator(after_access) || after_access->type == TOKEN_EOF)) {
                        print_statement.has_array_access = 1;
                        print_statement.value = NULL;
                        print_statement.is_variable_reference = 0;
                        print_statement.has_expression = 0;
                        print_statement.expression_text = NULL;
                        statement.type = STATEMENT_PRINT;
                        statement.as.print_statement = print_statement;
                        if (is_statement_terminator(after_access)) {
                            parser_advance(parser);
                        }
                        return statement;
                    }
                    array_access_free(&print_statement.array_access);
                }
            } else if (next_token == NULL || is_statement_terminator(next_token) || next_token->type == TOKEN_EOF) {
                print_statement.is_variable_reference = 1;
                print_statement.has_array_access = 0;
                print_statement.has_expression = 0;
                print_statement.expression_text = NULL;
                array_access_init(&print_statement.array_access);
                print_statement.value = drift_duplicate_string(token->value);
                parser_advance(parser);
                statement.type = STATEMENT_PRINT;
                statement.as.print_statement = print_statement;

                token = parser_peek(parser);
                if (token != NULL && is_statement_terminator(token)) {
                    parser_advance(parser);
                }
                return statement;
            }
        }

        size_t start_index = parser->index;
        size_t length = 0;
        Token *current = parser_peek(parser);
        while (current != NULL && !is_statement_terminator(current) && current->type != TOKEN_EOF) {
            length += strlen(current->value ? current->value : "") + 5U;
            current = &parser->tokens[parser->index + (size_t)(current - &parser->tokens[start_index]) + 1U];
        }

        print_statement.has_expression = 1;
        print_statement.expression_text = (char *)malloc(length + 1U);
        if (print_statement.expression_text == NULL) {
            fprintf(stderr, "Error: out of memory while reading expression.\n");
            statement.type = STATEMENT_PRINT;
            return statement;
        }
        print_statement.expression_text[0] = '\0';

        while (parser->index < parser->count && !is_statement_terminator(&parser->tokens[parser->index]) && parser->tokens[parser->index].type != TOKEN_EOF) {
            Token *tok = &parser->tokens[parser->index];
            if (print_statement.expression_text[0] != '\0') {
                strcat(print_statement.expression_text, " ");
            }
            if (tok->type == TOKEN_STRING) {
                if (tok->value != NULL && (tok->value[0] == '"' || tok->value[0] == '\'')) {
                    strcat(print_statement.expression_text, tok->value);
                } else {
                    strcat(print_statement.expression_text, "\"");
                    strcat(print_statement.expression_text, tok->value ? tok->value : "");
                    strcat(print_statement.expression_text, "\"");
                }
            } else if (tok->value != NULL) {
                strcat(print_statement.expression_text, tok->value);
            }
            parser_advance(parser);
        }

        token = parser_peek(parser);
        if (token != NULL && is_statement_terminator(token)) {
            parser_advance(parser);
        }

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

    if (token->type == TOKEN_IDENTIFIER && parser->index + 1 < parser->count &&
        (parser->tokens[parser->index + 1].type == TOKEN_EQUAL ||
         is_assignment_operator_token(parser->tokens[parser->index + 1].type))) {
        variable_declaration.vars = NULL;
        variable_declaration.count = 0;

        while (1) {
            VariableDeclarationSingle single;
            variable_declaration_single_init(&single);

            if (!parse_single_declaration(parser, &single, 0)) {
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
                fprintf(stderr, "Error: out of memory while parsing assignment\n");
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

    if (token->type == TOKEN_ASK) {
        if (!parse_ask_statement(parser, &statement)) {
            return statement;
        }
        return statement;
    }

    if (token->type == TOKEN_IF) {
        if (!parse_if_statement(parser, &statement)) {
            return statement;
        }
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
    free(statement->expression_text);
    statement->expression_text = NULL;
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
