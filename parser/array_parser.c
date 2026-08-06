#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/parser.h"
#include "drift/value.h"
#include "drift/array_value.h"

static Token *parser_peek(Parser *parser);
static Token *parser_advance(Parser *parser);

static int parse_integer_token(Token *token, long *out_value)
{
    if (token == NULL || token->type != TOKEN_INTEGER) {
        return 0;
    }

    char *end = NULL;
    long value = strtol(token->value, &end, 10);
    (void)end;
    *out_value = value;
    return 1;
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

static int is_literal_value_token(Token *token)
{
    return token != NULL && (token->type == TOKEN_INTEGER || token->type == TOKEN_FLOAT || token->type == TOKEN_STRING || token->type == TOKEN_TRUE || token->type == TOKEN_FALSE || token->type == TOKEN_NULL || token->type == TOKEN_INFINITY);
}

static Value parse_literal_value_token(Token *token)
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
                Value value = value_create_string(stripped);
                free(stripped);
                return value;
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

static int array_element_type_compatible(ValueType current, ValueType incoming)
{
    if (current == VALUE_NULL || current == VALUE_INFINITY) {
        return current == incoming;
    }
    return current == incoming;
}

static int array_update_element_type(ValueType *type, ValueType incoming)
{
    if (*type == VALUE_NULL && incoming != VALUE_NULL) {
        *type = incoming;
        return 1;
    }
    if (*type == VALUE_INFINITY && incoming != VALUE_INFINITY) {
        return 0;
    }
    return array_element_type_compatible(*type, incoming);
}

static int parse_initializer_values(Parser *parser,
                                    size_t level,
                                    size_t dimension_count,
                                    const long *dimensions,
                                    ValueType *element_type,
                                    Value **out_values,
                                    size_t *out_count)
{
    Token *token = parser_peek(parser);
    if (token == NULL) {
        fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
        return 0;
    }

    if (token->type != TOKEN_LEFT_BRACE) {
        fprintf(stderr, "Syntax Error: Expected '{' in array initializer.\n");
        return 0;
    }
    parser_advance(parser);

    Value *values = NULL;
    size_t count = 0;
    size_t expected_children = (level < dimension_count ? (size_t)dimensions[level] : 0);
    size_t child_count = 0;

    while (1) {
        token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_BRACE) {
            break;
        }

        if (level + 1 == dimension_count) {
            if (!is_literal_value_token(token)) {
                fprintf(stderr, "Syntax Error: Expected literal value in array initializer.\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            Value value = parse_literal_value_token(token);
            if (count == 0) {
                *element_type = value.type;
            } else if (!array_update_element_type(element_type, value.type)) {
                value_free(&value);
                fprintf(stderr, "Type Error: Array elements must all have the same data type.\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            parser_advance(parser);
            Value *new_values = (Value *)realloc(values, (count + 1U) * sizeof(Value));
            if (new_values == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                value_free(&value);
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }
            values = new_values;
            values[count++] = value;
        } else {
            if (token->type != TOKEN_LEFT_BRACE) {
                fprintf(stderr, "Syntax Error: Expected '{' in nested array initializer.\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            Value *child_values = NULL;
            size_t child_count_local = 0;
            if (!parse_initializer_values(parser, level + 1, dimension_count, dimensions, element_type, &child_values, &child_count_local)) {
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            Value *new_values = (Value *)realloc(values, (count + child_count_local) * sizeof(Value));
            if (new_values == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                for (size_t i = 0; i < child_count_local; ++i) {
                    value_free(&child_values[i]);
                }
                free(child_values);
                return 0;
            }

            values = new_values;
            for (size_t i = 0; i < child_count_local; ++i) {
                values[count + i] = child_values[i];
            }
            count += child_count_local;
            free(child_values);
            child_count++;
        }

        token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        if (token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (token->type == TOKEN_RIGHT_BRACE) {
            break;
        }

        if (token->type == TOKEN_LEFT_BRACE && level + 1 < dimension_count && child_count < expected_children) {
            continue;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or '}' in array initializer.\n");
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        return 0;
    }

    if (!parser_expect(parser, TOKEN_RIGHT_BRACE, "Syntax Error: Unterminated array initializer.")) {
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        return 0;
    }

    *out_values = values;
    *out_count = count;
    return 1;
}

static int parse_array_dimensions(Parser *parser, long **out_dimensions, size_t *out_count, int *error, int *out_dynamic_declaration)
{
    size_t count = 0;
    long *dimensions = NULL;
    int dynamic_declaration = 0;

    while (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_LEFT_BRACKET) {
        parser_advance(parser);

        Token *token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_RIGHT_BRACKET) {
            parser_advance(parser);
            dynamic_declaration = 1;
            break;
        }

        if (token == NULL || token->type != TOKEN_INTEGER) {
            fprintf(stderr, "Syntax Error: Expected array size after '['.\n");
            free(dimensions);
            *error = 1;
            return 0;
        }

        long size = 0;
        if (!parse_integer_token(token, &size)) {
            fprintf(stderr, "Syntax Error: Invalid array size.\n");
            free(dimensions);
            *error = 1;
            return 0;
        }

        parser_advance(parser);
        if (!parser_expect(parser, TOKEN_RIGHT_BRACKET, "Syntax Error: Expected ']' after array size.")) {
            free(dimensions);
            *error = 1;
            return 0;
        }

        long *new_dimensions = (long *)realloc(dimensions, (count + 1U) * sizeof(long));
        if (new_dimensions == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array dimensions\n");
            free(dimensions);
            *error = 1;
            return 0;
        }
        dimensions = new_dimensions;
        dimensions[count++] = size;
    }

    *out_dimensions = dimensions;
    *out_count = count;
    if (out_dynamic_declaration != NULL) {
        *out_dynamic_declaration = dynamic_declaration;
    }
    return 1;
}

static int parse_multi_dimensional_initializer(Parser *parser,
                                              size_t dimension_count,
                                              const long *dimensions,
                                              ValueType *element_type,
                                              Value **out_values,
                                              size_t *out_count)
{
    Value *values = NULL;
    size_t count = 0;

    for (long block = 0; block < dimensions[0]; ++block) {
        Value *block_values = NULL;
        size_t block_count = 0;
        if (!parse_initializer_values(parser, 1, dimension_count, dimensions, element_type, &block_values, &block_count)) {
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        Value *new_values = (Value *)realloc(values, (count + block_count) * sizeof(Value));
        if (new_values == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            for (size_t i = 0; i < block_count; ++i) {
                value_free(&block_values[i]);
            }
            free(block_values);
            return 0;
        }

        values = new_values;
        for (size_t i = 0; i < block_count; ++i) {
            values[count + i] = block_values[i];
        }
        count += block_count;
        free(block_values);
    }

    *out_values = values;
    *out_count = count;
    return 1;
}

static int parse_dynamic_initializer_values(Parser *parser,
                                            ValueType *element_type,
                                            Value **out_values,
                                            size_t *out_count)
{

    size_t count = 0;
    Value *values = NULL;

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_BRACE) {
            parser_advance(parser);
            break;
        }

        if (!is_literal_value_token(token)) {
            fprintf(stderr, "Syntax Error: Expected literal value in array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        Value value = parse_literal_value_token(token);
        if (count == 0) {
            *element_type = value.type;
        } else if (!array_update_element_type(element_type, value.type)) {
            value_free(&value);
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            fprintf(stderr, "Type Error: Array elements must all have the same data type.\n");
            return 0;
        }

        parser_advance(parser);
        Value *new_values = (Value *)realloc(values, (count + 1U) * sizeof(Value));
        if (new_values == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            value_free(&value);
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }
        values = new_values;
        values[count++] = value;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (token != NULL && token->type == TOKEN_RIGHT_BRACE) {
            continue;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or '}' in array initializer.\n");
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        return 0;
    }

    *out_values = values;
    *out_count = count;
    return 1;
}

static int array_declaration_has_initializer(Parser *parser)
{
    Token *token = parser_peek(parser);
    return token != NULL && token->type == TOKEN_EQUAL;
}

Value parse_array_declaration(Parser *parser, int *error)
{
    long *dimensions = NULL;
    size_t dimension_count = 0;
    int dynamic_declaration = 0;
    Value result = value_create_string(NULL);
    if (!parse_array_dimensions(parser, &dimensions, &dimension_count, error, &dynamic_declaration)) {
        return result;
    }

    if (dynamic_declaration) {
        ArrayValue *array = array_value_create_empty();
        free(dimensions);
        result = value_create_array(array);
        *error = 0;
        return result;
    }

    if (!array_declaration_has_initializer(parser) && dimension_count == 0) {
        fprintf(stderr, "Syntax Error: Expected '=' or array dimensions in variable declaration.\n");
        free(dimensions);
        *error = 1;
        return result;
    }

    if (array_declaration_has_initializer(parser)) {
        parser_advance(parser);

        if (dimension_count == 0) {
            Value *values = NULL;
            size_t value_count = 0;
            ValueType element_type = VALUE_NULL;
            if (!parse_dynamic_initializer_values(parser, &element_type, &values, &value_count)) {
                free(dimensions);
                *error = 1;
                return result;
            }

            if (value_count == 0) {
                ArrayValue *array = array_value_create_empty();
                result = value_create_array(array);
                *error = 0;
                free(dimensions);
                return result;
            }

            long *dynamic_dimensions = (long *)malloc(sizeof(long));
            if (dynamic_dimensions == NULL) {
                for (size_t i = 0; i < value_count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                free(dimensions);
                *error = 1;
                return result;
            }
            dynamic_dimensions[0] = (long)value_count;

            ArrayValue *array = array_value_create_from_values(1, dynamic_dimensions, element_type, values, value_count);
            for (size_t j = 0; j < value_count; ++j) {
                value_free(&values[j]);
            }
            free(values);
            free(dynamic_dimensions);
            free(dimensions);
            result = value_create_array(array);
            *error = 0;
            return result;
        }

        Value *values = NULL;
        size_t value_count = 0;
        ValueType element_type = VALUE_NULL;
        if (!parse_multi_dimensional_initializer(parser, dimension_count, dimensions, &element_type, &values, &value_count)) {
            free(dimensions);
            *error = 1;
            return result;
        }

        size_t total_count = 1;
        for (size_t i = 0; i < dimension_count; ++i) {
            if (dimensions[i] < 0) {
                fprintf(stderr, "Runtime Error: Array size cannot be negative.\n");
                for (size_t j = 0; j < value_count; ++j) {
                    value_free(&values[j]);
                }
                free(values);
                free(dimensions);
                *error = 1;
                return result;
            }
            total_count *= (size_t)dimensions[i];
        }

        if (value_count == 0) {
            ArrayValue *array = array_value_create_fixed(dimension_count, dimensions, VALUE_NULL);
            free(dimensions);
            free(values);
            result = value_create_array(array);
            *error = 0;
            return result;
        }

        if (value_count != total_count) {
            fprintf(stderr, "Syntax Error: Expected %zu elements but found %zu.\n", total_count, value_count);
            for (size_t j = 0; j < value_count; ++j) {
                value_free(&values[j]);
            }
            free(values);
            free(dimensions);
            *error = 1;
            return result;
        }

        ArrayValue *array = array_value_create_from_values(dimension_count, dimensions, element_type, values, value_count);
        for (size_t j = 0; j < value_count; ++j) {
            value_free(&values[j]);
        }
        free(values);
        free(dimensions);
        result = value_create_array(array);
        *error = 0;
        return result;
    }

    for (size_t i = 0; i < dimension_count; ++i) {
        if (dimensions[i] < 0) {
            fprintf(stderr, "Runtime Error: Array size cannot be negative.\n");
            free(dimensions);
            *error = 1;
            return result;
        }
    }

    ArrayValue *array = array_value_create_fixed(dimension_count, dimensions, VALUE_NULL);
    free(dimensions);
    result = value_create_array(array);
    *error = 0;
    return result;
}

static int parse_integer_sequence(Parser *parser, long **out_indices, size_t *out_count, int *error)
{
    if (error != NULL) {
        *error = 0;
    }
    size_t count = 0;
    long *indices = NULL;

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL || token->type != TOKEN_INTEGER) {
            fprintf(stderr, "Syntax Error: Expected integer index in array access.\n");
            free(indices);
            *error = 1;
            return 0;
        }

        long index = 0;
        if (!parse_integer_token(token, &index)) {
            fprintf(stderr, "Syntax Error: Invalid array index.\n");
            free(indices);
            *error = 1;
            return 0;
        }

        parser_advance(parser);
        long *new_indices = (long *)realloc(indices, (count + 1U) * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading array index\n");
            free(indices);
            *error = 1;
            return 0;
        }
        indices = new_indices;
        indices[count++] = index;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        break;
    }

    *out_indices = indices;
    *out_count = count;
    return 1;
}

int parse_array_access(Parser *parser, ArrayAccess *access)
{
    array_access_init(access);
    Token *token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER) {
        return 0;
    }

    access->name = drift_duplicate_string(token->value);
    parser_advance(parser);

    while (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_LEFT_BRACKET) {
        parser_advance(parser);
        Token *next = parser_peek(parser);
        if (next == NULL) {
            fprintf(stderr, "Syntax Error: Expected array access content.\n");
            return 0;
        }

        if (next->type == TOKEN_RIGHT_BRACKET) {
            parser_advance(parser);
            access->is_whole_array = 1;
            return 1;
        }

        if (next->type == TOKEN_LEFT_PAREN) {
            fprintf(stderr, "Syntax Error: Use select() for multi-value array access.\n");
            return 0;
        }

        long index = 0;
        if (!parse_integer_token(next, &index)) {
            fprintf(stderr, "Syntax Error: Invalid array index.\n");
            return 0;
        }
        parser_advance(parser);

        if (!parser_expect(parser, TOKEN_RIGHT_BRACKET, "Syntax Error: Expected ']' after array index.")) {
            return 0;
        }

        long *new_indices = (long *)realloc(access->indices, (access->index_count + 1U) * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading array indices\n");
            return 0;
        }
        access->indices = new_indices;
        access->indices[access->index_count++] = index;
    }

    if (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_DOT) {
        return parse_select_access(parser, access);
    }

    return 1;
}

void array_access_init(ArrayAccess *access)
{
    if (access == NULL) {
        return;
    }

    access->name = NULL;
    access->is_whole_array = 0;
    access->is_selection = 0;
    access->index_count = 0;
    access->indices = NULL;
    access->selection_count = 0;
    access->selection_tuple_size = 0;
    access->selection_indices = NULL;
    access->selection_breaks = NULL;
}

void array_access_free(ArrayAccess *access)
{
    if (access == NULL) {
        return;
    }

    free(access->name);
    free(access->indices);
    free(access->selection_indices);
    free(access->selection_breaks);
    access->name = NULL;
    access->indices = NULL;
    access->selection_indices = NULL;
    access->selection_breaks = NULL;
}

Token *parser_peek(Parser *parser)
{
    if (parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

Token *parser_advance(Parser *parser)
{
    if (parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index++];
}
