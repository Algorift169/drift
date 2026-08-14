#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/array_value.h"
#include "drift/input.h"
#include "drift/interpreter.h"
#include "drift/lexer.h"
#include "drift/parser.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

static int sb_init(StringBuilder *sb)
{
    if (sb == NULL) {
        return 0;
    }

    sb->capacity = 64;
    sb->length = 0;
    sb->data = (char *)malloc(sb->capacity);
    if (sb->data == NULL) {
        return 0;
    }
    sb->data[0] = '\0';
    return 1;
}

static void sb_free(StringBuilder *sb)
{
    if (sb == NULL) {
        return;
    }
    free(sb->data);
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

static int sb_append(StringBuilder *sb, const char *text)
{
    if (sb == NULL || text == NULL) {
        return 0;
    }

    size_t text_length = strlen(text);
    size_t required = sb->length + text_length + 1;
    if (required > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        char *new_data = (char *)realloc(sb->data, new_capacity);
        if (new_data == NULL) {
            return 0;
        }
        sb->data = new_data;
        sb->capacity = new_capacity;
    }

    memcpy(sb->data + sb->length, text, text_length);
    sb->length += text_length;
    sb->data[sb->length] = '\0';
    return 1;
}

static int sb_append_char(StringBuilder *sb, char c)
{
    if (sb == NULL) {
        return 0;
    }

    if (sb->length + 2 > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        char *new_data = (char *)realloc(sb->data, new_capacity);
        if (new_data == NULL) {
            return 0;
        }
        sb->data = new_data;
        sb->capacity = new_capacity;
    }

    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
    return 1;
}

static int format_value_to_string(const Value *value, StringBuilder *sb);
static int format_array_value_to_string(const ArrayValue *array, StringBuilder *sb);
static int format_array_recursive_to_string(const ArrayValue *array, size_t dimension, size_t base_offset, StringBuilder *sb);
static int format_array_element_to_string(const ArrayValue *array, const long *indices, size_t index_count, StringBuilder *sb);

static int resolve_array_indices(const ArrayAccess *access, Environment *environment, long **out_indices)
{
    if (access == NULL || environment == NULL || out_indices == NULL) {
        return 0;
    }

    long *indices = NULL;
    if (access->index_count > 0) {
        indices = (long *)malloc(access->index_count * sizeof(long));
        if (indices == NULL) {
            fprintf(stderr, "Error: out of memory while resolving array indices\n");
            return 0;
        }
    }

    for (size_t i = 0; i < access->index_count; ++i) {
        indices[i] = access->indices[i];
        if (access->index_names != NULL && access->index_names[i] != NULL) {
            Value index_value;
            if (!environment_get(environment, access->index_names[i], &index_value)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", access->index_names[i]);
                free(indices);
                return 0;
            }
            if (index_value.type != VALUE_INTEGER) {
                fprintf(stderr, "Runtime Error: Array index variable '%s' must be an integer.\n", access->index_names[i]);
                value_free(&index_value);
                free(indices);
                return 0;
            }
            indices[i] = index_value.integer_value;
            value_free(&index_value);
        }
    }

    *out_indices = indices;
    return 1;
}

static int parse_array_template_expression(const char *expr, ArrayAccess *access)
{
    if (expr == NULL || access == NULL) {
        return 0;
    }

    array_access_init(access);
    Lexer lexer = lexer_create(expr);
    size_t token_count = 0;
    Token *tokens = lexer_scan_all(&lexer, &token_count);
    if (tokens == NULL) {
        return 0;
    }

    Parser parser = parser_create(tokens, token_count);
    int parsed = parse_array_access(&parser, access);
    int is_access = access->is_whole_array || access->is_selection || access->index_count > 0;
    int consumed_all = parser.index < parser.count && parser.tokens[parser.index].type == TOKEN_EOF;
    token_free_array(tokens, token_count);

    if (!parsed || !is_access || !consumed_all) {
        array_access_free(access);
        return 0;
    }

    return 1;
}

static int value_is_truthy(const Value *value)
{
    if (value == NULL) {
        return 0;
    }

    if (value->type == VALUE_BOOLEAN) {
        return value->boolean_value;
    }
    if (value->type == VALUE_INTEGER) {
        return value->integer_value != 0;
    }
    if (value->type == VALUE_FLOAT) {
        return value->float_value != 0.0;
    }
    if (value->type == VALUE_STRING) {
        return value->string_value != NULL && value->string_value[0] != '\0';
    }
    if (value->type == VALUE_NULL) {
        return 0;
    }
    if (value->type == VALUE_INFINITY) {
        return 1;
    }
    if (value->type == VALUE_ARRAY) {
        return value->array_value != NULL && value->array_value->length > 0;
    }
    return 0;
}

static Value resolve_identifier_or_literal(Environment *environment, Token *token, int *ok)
{
    Value value = value_create_null();
    if (token == NULL || ok == NULL) {
        *ok = 0;
        return value;
    }

    *ok = 1;
    if (token->type == TOKEN_INTEGER) {
        char *end = NULL;
        long v = strtol(token->value, &end, 10);
        (void)end;
        return value_create_integer(v);
    }
    if (token->type == TOKEN_FLOAT) {
        char *end = NULL;
        double v = strtod(token->value, &end);
        (void)end;
        return value_create_float(v);
    }
    if (token->type == TOKEN_STRING) {
        if (token->value != NULL && (token->value[0] == '"' || token->value[0] == '\'')) {
            size_t len = strlen(token->value);
            if (len >= 2 && token->value[len - 1] == token->value[0]) {
                char *unquoted = (char *)malloc(len - 1);
                if (unquoted != NULL) {
                    memcpy(unquoted, token->value + 1, len - 2);
                    unquoted[len - 2] = '\0';
                    Value v = value_create_string(unquoted);
                    free(unquoted);
                    return v;
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
    if (token->type == TOKEN_IDENTIFIER) {
        if (!environment_get(environment, token->value, &value)) {
            fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", token->value);
            *ok = 0;
            return value_create_null();
        }
        return value;
    }

    *ok = 0;
    return value_create_null();
}

static int token_precedence(TokenType type)
{
    switch (type) {
        case TOKEN_RANGE:
            return 1;
        case TOKEN_OR_OR:
            return 2;
        case TOKEN_AND_AND:
            return 3;
        case TOKEN_PIPE:
            return 4;
        case TOKEN_CARET:
            return 5;
        case TOKEN_AMPERSAND:
            return 6;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
            return 7;
        case TOKEN_GREATER:
        case TOKEN_LESS:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_LESS_EQUAL:
            return 8;
        case TOKEN_SHIFT_LEFT:
        case TOKEN_SHIFT_RIGHT:
            return 9;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 10;
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            return 11;
        case TOKEN_IN:
            return 12;
        default:
            return -1;
    }
}

static Value apply_unary_operator(TokenType type, const Value *value)
{
    Value result = value_create_null();
    if (value == NULL) {
        return result;
    }

    if (type == TOKEN_MINUS) {
        if (value->type == VALUE_INTEGER) {
            return value_create_integer(-value->integer_value);
        }
        if (value->type == VALUE_FLOAT) {
            return value_create_float(-value->float_value);
        }
        return value_create_null();
    }
    if (type == TOKEN_BANG) {
        return value_create_boolean(!value_is_truthy(value));
    }
    if (type == TOKEN_TILDA) {
        if (value->type == VALUE_INTEGER) {
            return value_create_integer(~value->integer_value);
        }
        return value_create_null();
    }
    if (type == TOKEN_PLUS) {
        return value_copy(value);
    }
    return result;
}

static Value evaluate_expression_tokens(Environment *environment, Token *tokens, size_t count, size_t *index, int min_precedence, int *ok)
{
    Value left = value_create_null();
    TokenType unary_type = TOKEN_UNKNOWN;
    Token *token = NULL;

    if (tokens == NULL || index == NULL || ok == NULL) {
        *ok = 0;
        return left;
    }

    if (*index >= count) {
        *ok = 0;
        return left;
    }

    token = &tokens[*index];
    if (token->type == TOKEN_PLUS || token->type == TOKEN_MINUS || token->type == TOKEN_BANG || token->type == TOKEN_TILDA) {
        unary_type = token->type;
        (*index)++;
    }

    if (*index >= count) {
        *ok = 0;
        return left;
    }

    token = &tokens[*index];
    if (token->type == TOKEN_LEFT_PAREN) {
        (*index)++;
        left = evaluate_expression_tokens(environment, tokens, count, index, 0, ok);
        if (!*ok || (*index >= count) || tokens[*index].type != TOKEN_RIGHT_PAREN) {
            *ok = 0;
            return left;
        }
        (*index)++;
    } else if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_INTEGER || token->type == TOKEN_FLOAT ||
               token->type == TOKEN_STRING || token->type == TOKEN_TRUE || token->type == TOKEN_FALSE ||
               token->type == TOKEN_NULL || token->type == TOKEN_INFINITY) {
        left = resolve_identifier_or_literal(environment, token, ok);
        (*index)++;
        while (*index < count && tokens[*index].type == TOKEN_LEFT_BRACKET) {
            size_t bracket_index = *index;
            Value index_value;
            Value array_value;
            long arr_index = 0;
            (*index)++;
            index_value = evaluate_expression_tokens(environment, tokens, count, index, 0, ok);
            if (!*ok) {
                return left;
            }
            if (index_value.type != VALUE_INTEGER) {
                fprintf(stderr, "Runtime Error: Array index must be an integer.\n");
                *ok = 0;
                return left;
            }
            arr_index = index_value.integer_value;
            value_free(&index_value);
            if (*index >= count || tokens[*index].type != TOKEN_RIGHT_BRACKET) {
                fprintf(stderr, "Runtime Error: Expected ']' after array index.\n");
                *ok = 0;
                return left;
            }
            (*index)++;
            if (left.type != VALUE_ARRAY) {
                fprintf(stderr, "Runtime Error: Only array values can be indexed.\n");
                *ok = 0;
                return left;
            }
            if (!environment_get(environment, left.string_value ? left.string_value : "", &array_value)) {
                fprintf(stderr, "Runtime Error: Invalid array access.\n");
                *ok = 0;
                return left;
            }
            if (array_value.type == VALUE_ARRAY && array_value.array_value != NULL && arr_index >= 0 && (size_t)arr_index < array_value.array_value->length) {
                left = value_copy(&array_value.array_value->elements[arr_index]);
            } else {
                fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                *ok = 0;
                value_free(&array_value);
                return left;
            }
            value_free(&array_value);
            (void)bracket_index;
        }
    } else {
        *ok = 0;
        return left;
    }

    if (unary_type != TOKEN_UNKNOWN) {
        left = apply_unary_operator(unary_type, &left);
    }

    while (*index < count) {
        TokenType op = tokens[*index].type;
        if (op == TOKEN_TILDA) {
            fprintf(stderr, "Runtime Error: Bitwise NOT is unary and must be written as '~value'.\n");
            *ok = 0;
            return left;
        }

        int precedence = token_precedence(op);
        if (precedence < min_precedence) {
            break;
        }

        (*index)++;
        if (*index >= count) {
            *ok = 0;
            return left;
        }

        Value right = evaluate_expression_tokens(environment, tokens, count, index, precedence + 1, ok);
        if (!*ok) {
            return left;
        }

        if (op == TOKEN_IN) {
            left = operator_apply(OPERATOR_IN, &left, &right);
        } else if (op == TOKEN_RANGE) {
            left = operator_apply(OPERATOR_RANGE, &left, &right);
        } else if (op == TOKEN_PLUS) {
            left = operator_apply(OPERATOR_ADD, &left, &right);
        } else if (op == TOKEN_MINUS) {
            left = operator_apply(OPERATOR_SUBTRACT, &left, &right);
        } else if (op == TOKEN_STAR) {
            left = operator_apply(OPERATOR_MULTIPLY, &left, &right);
        } else if (op == TOKEN_SLASH) {
            left = operator_apply(OPERATOR_DIVIDE, &left, &right);
        } else if (op == TOKEN_PERCENT) {
            left = operator_apply(OPERATOR_MODULO, &left, &right);
        } else if (op == TOKEN_EQUAL_EQUAL) {
            left = operator_apply(OPERATOR_EQUAL_EQUAL, &left, &right);
        } else if (op == TOKEN_NOT_EQUAL) {
            left = operator_apply(OPERATOR_NOT_EQUAL, &left, &right);
        } else if (op == TOKEN_GREATER) {
            left = operator_apply(OPERATOR_GREATER, &left, &right);
        } else if (op == TOKEN_LESS) {
            left = operator_apply(OPERATOR_LESS, &left, &right);
        } else if (op == TOKEN_GREATER_EQUAL) {
            left = operator_apply(OPERATOR_GREATER_EQUAL, &left, &right);
        } else if (op == TOKEN_LESS_EQUAL) {
            left = operator_apply(OPERATOR_LESS_EQUAL, &left, &right);
        } else if (op == TOKEN_AND_AND) {
            left = operator_apply(OPERATOR_AND_AND, &left, &right);
        } else if (op == TOKEN_OR_OR) {
            left = operator_apply(OPERATOR_OR_OR, &left, &right);
        } else if (op == TOKEN_AMPERSAND) {
            left = operator_apply(OPERATOR_BITWISE_AND, &left, &right);
        } else if (op == TOKEN_PIPE) {
            left = operator_apply(OPERATOR_BITWISE_OR, &left, &right);
        } else if (op == TOKEN_CARET) {
            left = operator_apply(OPERATOR_BITWISE_XOR, &left, &right);
        } else if (op == TOKEN_SHIFT_LEFT) {
            left = operator_apply(OPERATOR_SHIFT_LEFT, &left, &right);
        } else if (op == TOKEN_SHIFT_RIGHT) {
            left = operator_apply(OPERATOR_SHIFT_RIGHT, &left, &right);
        } else if (op == TOKEN_TILDA) {
            fprintf(stderr, "Runtime Error: Bitwise NOT is unary and must be written as '~value'.\n");
            *ok = 0;
            value_free(&right);
            return left;
        } else {
            *ok = 0;
            return left;
        }

        value_free(&right);
    }

    return left;
}

static Value evaluate_expression_text(Environment *environment, const char *expression, int *ok)
{
    Lexer lexer;
    Token *tokens = NULL;
    size_t token_count = 0;
    Value result = value_create_null();
    size_t index = 0;

    if (expression == NULL || ok == NULL) {
        if (ok != NULL) {
            *ok = 0;
        }
        return result;
    }

    lexer = lexer_create(expression);
    tokens = lexer_scan_all(&lexer, &token_count);
    if (tokens == NULL) {
        *ok = 0;
        return result;
    }

    *ok = 1;
    result = evaluate_expression_tokens(environment, tokens, token_count, &index, 0, ok);
    if (*ok && index < token_count && tokens[index].type != TOKEN_EOF) {
        *ok = 0;
    }

    token_free_array(tokens, token_count);
    return result;
}

static void print_value(const Value *value)
{
    if (value == NULL) {
        return;
    }

    switch (value->type) {
        case VALUE_INTEGER:
            printf("%ld", value->integer_value);
            break;
        case VALUE_FLOAT:
            printf("%g", value->float_value);
            break;
        case VALUE_STRING:
            printf("%s", value->string_value ? value->string_value : "");
            break;
        case VALUE_BOOLEAN:
            printf(value->boolean_value ? "true" : "false");
            break;
        case VALUE_NULL:
            printf("null");
            break;
        case VALUE_INFINITY:
            printf("infinity");
            break;
        case VALUE_ARRAY:
            print_array_value(value->array_value);
            break;
        default:
            break;
    }
}

static int format_value_to_string(const Value *value, StringBuilder *sb)
{
    char buffer[128];

    if (value == NULL || sb == NULL) {
        return 0;
    }

    switch (value->type) {
        case VALUE_INTEGER:
            snprintf(buffer, sizeof(buffer), "%ld", value->integer_value);
            return sb_append(sb, buffer);
        case VALUE_FLOAT:
            snprintf(buffer, sizeof(buffer), "%g", value->float_value);
            return sb_append(sb, buffer);
        case VALUE_STRING:
            return sb_append(sb, value->string_value ? value->string_value : "");
        case VALUE_BOOLEAN:
            return sb_append(sb, value->boolean_value ? "true" : "false");
        case VALUE_NULL:
            return sb_append(sb, "null");
        case VALUE_INFINITY:
            return sb_append(sb, "infinity");
        case VALUE_ARRAY:
            return format_array_value_to_string(value->array_value, sb);
        default:
            return 0;
    }
}

static int format_array_recursive_to_string(const ArrayValue *array, size_t dimension, size_t base_offset, StringBuilder *sb)
{
    if (array == NULL || sb == NULL) {
        return 0;
    }

    if (dimension + 1 == array->dimension_count) {
        for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
            if (i > 0) {
                if (!sb_append_char(sb, ',')) {
                    return 0;
                }
            }
            if (!format_value_to_string(&array->elements[base_offset + i], sb)) {
                return 0;
            }
        }
        return 1;
    }

    size_t stride = 1;
    for (size_t i = dimension + 1; i < array->dimension_count; ++i) {
        stride *= (size_t)array->dimensions[i];
    }

    for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
        if(i > 0) {
            if (!sb_append_char(sb, '\n')) {
                return 0;
            }
            if(dimension == 0 && array->dimension_count > 2) {
                if (!sb_append_char(sb, '\n')) {
                    return 0;
                }
            }
        }
        if (!format_array_recursive_to_string(array, dimension + 1, base_offset + i * stride, sb)) {
            return 0;
        }
    }

    return 1;
}

static int format_array_value_to_string(const ArrayValue *array, StringBuilder *sb)
{
    if (array == NULL || sb == NULL) {
        return 0;
    }

    if (array->is_dynamic && array->length == 0) {
        return 1;
    }

    return format_array_recursive_to_string(array, 0, 0, sb);
}

static int format_array_element_to_string(const ArrayValue *array, const long *indices, size_t index_count, StringBuilder *sb)
{
    int error = 0;
    const Value *value = array_value_get_element(array, indices, index_count, &error);
    if (value == NULL) {
        if (error) {
            fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
        }
        return 0;
    }

    return format_value_to_string(value, sb);
}

static char *interpolate_template(const char *template, Environment *environment)
{
    char *result;
    size_t result_capacity = 64;
    size_t result_length = 0;
    size_t i = 0;

    if (template == NULL) {
        return NULL;
    }

    result = (char *)malloc(result_capacity);
    if (result == NULL) {
        fprintf(stderr, "Error: out of memory while interpolating template\n");
        return NULL;
    }
    result[0] = '\0';

    while (template[i] != '\0') {
        if (template[i] == '{' && template[i + 1] != '\0') {
            size_t start = i + 1;
            size_t end = start;
            char *name;
            Value value;
            char *formatted = NULL;
            size_t formatted_length = 0;

            while (template[end] != '\0' && template[end] != '}') {
                end++;
            }

            if (template[end] == '\0') {
                free(result);
                return NULL;
            }

            name = (char *)malloc(end - start + 1U);
            if (name == NULL) {
                free(result);
                return NULL;
            }
            memcpy(name, template + start, end - start);
            name[end - start] = '\0';

            ArrayAccess array_access;
            int has_array_template = parse_array_template_expression(name, &array_access);
            if (has_array_template) {
                Value array_value;
                if (!environment_get(environment, array_access.name, &array_value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", array_access.name);
                    array_access_free(&array_access);
                    free(name);
                    free(result);
                    return NULL;
                }

                if (array_value.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", array_access.name);
                    value_free(&array_value);
                    array_access_free(&array_access);
                    free(name);
                    free(result);
                    return NULL;
                }

                StringBuilder sb;
                if (!sb_init(&sb)) {
                    fprintf(stderr, "Error: out of memory while interpolating template\n");
                    value_free(&array_value);
                    array_access_free(&array_access);
                    free(name);
                    free(result);
                    return NULL;
                }

                int success;
                if (array_access.is_whole_array) {
                    if (array_access.whole_array_dimension_count != array_value.array_value->dimension_count) {
                        fprintf(stderr, "Runtime Error: Array rank is %zu; use %zu empty array access selector%s.\n",
                                array_value.array_value->dimension_count, array_value.array_value->dimension_count,
                                array_value.array_value->dimension_count == 1 ? "" : "s");
                        success = 0;
                    } else {
                        success = format_array_value_to_string(array_value.array_value, &sb);
                    }
                } else if (array_access.is_selection) {
                    success = array_access.selection_tuple_size == array_value.array_value->dimension_count;
                    if (!success) {
                        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %zu.\n",
                                array_access.selection_tuple_size, array_value.array_value->dimension_count);
                    }
                    for (size_t i = 0; success && i < array_access.selection_count; ++i) {
                        if (i > 0 && !sb_append_char(&sb, ' ')) {
                            success = 0;
                            break;
                        }
                        success = format_array_element_to_string(array_value.array_value,
                                                                 array_access.selection_indices + i * array_access.selection_tuple_size,
                                                                 array_access.selection_tuple_size, &sb);
                    }
                } else {
                    success = format_array_element_to_string(array_value.array_value, array_access.indices, array_access.index_count, &sb);
                }

                value_free(&array_value);
                array_access_free(&array_access);
                if (!success) {
                    sb_free(&sb);
                    free(name);
                    free(result);
                    return NULL;
                }

                formatted = sb.data;
            } else {
                if (!environment_get(environment, name, &value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", name);
                    free(name);
                    free(result);
                    return NULL;
                }

                formatted = (char *)malloc(128);
                if (formatted == NULL) {
                    value_free(&value);
                    free(name);
                    free(result);
                    return NULL;
                }

                formatted[0] = '\0';
                if (value.type == VALUE_INTEGER) {
                    snprintf(formatted, 128, "%ld", value.integer_value);
                } else if (value.type == VALUE_FLOAT) {
                    snprintf(formatted, 128, "%g", value.float_value);
                } else if (value.type == VALUE_STRING) {
                    snprintf(formatted, 128, "%s", value.string_value ? value.string_value : "");
                } else if (value.type == VALUE_BOOLEAN) {
                    snprintf(formatted, 128, "%s", value.boolean_value ? "true" : "false");
                } else if (value.type == VALUE_NULL) {
                    snprintf(formatted, 128, "null");
                } else if (value.type == VALUE_INFINITY) {
                    snprintf(formatted, 128, "infinity");
                } else if (value.type == VALUE_ARRAY) {
                    fprintf(stderr, "Syntax Error: Array variable '%s' must be accessed explicitly.\n", name);
                    value_free(&value);
                    free(name);
                    free(formatted);
                    free(result);
                    return NULL;
                }
                value_free(&value);
            }

            formatted_length = strlen(formatted);
            while (result_length + formatted_length + 1 > result_capacity) {
                result_capacity *= 2;
                result = (char *)realloc(result, result_capacity);
                if (result == NULL) {
                    fprintf(stderr, "Error: out of memory while interpolating template\n");
                    free(name);
                    free(formatted);
                    return NULL;
                }
            }

            strcat(result, formatted);
            result_length += formatted_length;
            i = end + 1;
            free(name);
            free(formatted);
            continue;
        }

        while (result_length + 2 > result_capacity) {
            result_capacity *= 2;
            result = (char *)realloc(result, result_capacity);
            if (result == NULL) {
                fprintf(stderr, "Error: out of memory while interpolating template\n");
                return NULL;
            }
        }

        result[result_length++] = template[i++];
        result[result_length] = '\0';
    }

    return result;
}

static int resolve_input_target_name(const char *target_name, Environment *environment, Value *out_value)
{
    if (target_name == NULL || out_value == NULL) {
        return 0;
    }

    if (!environment_get(environment, target_name, out_value)) {
        *out_value = value_create_string("");
    }

    return 1;
}

int interpreter_execute(Statement statement, Environment *environment)
{
    if (environment == NULL) {
        fprintf(stderr, "Runtime Error: missing execution environment.\n");
        return 1;
    }

    if (statement.type == STATEMENT_INPUT) {
        InputStatement *input_statement = &statement.as.input_statement;

        if (input_statement->items == NULL || input_statement->count == 0) {
            fprintf(stderr, "Runtime Error: Empty input statement.\n");
            return 1;
        }

        for (size_t i = 0; i < input_statement->count; ++i) {
            InputItem *item = &input_statement->items[i];
            Value value;
            char *resolved_prompt = NULL;
            char *target_name = item->has_target ? item->target_name : "__temp_input__";

            if (item->has_prompt) {
                resolved_prompt = interpolate_template(item->prompt, environment);
                if (resolved_prompt == NULL) {
                    return 1;
                }
            }

            if (!drift_prompt_and_store(resolved_prompt, target_name, &value)) {
                free(resolved_prompt);
                fprintf(stderr, "Runtime Error: Unable to read input for '%s'.\n", target_name);
                return 1;
            }

            if (item->has_target) {
                if (!environment_set(environment, item->target_name, &value)) {
                    value_free(&value);
                    free(resolved_prompt);
                    fprintf(stderr, "Runtime Error: Unable to store input in variable '%s'.\n", item->target_name);
                    return 1;
                }
            }

            value_free(&value);
            free(resolved_prompt);
        }

        return 0;
    }

    if (statement.type == STATEMENT_PRINT) {
        PrintStatement *print_statement = &statement.as.print_statement;
        Value value;
        char *resolved = NULL;

        if (print_statement->has_expression) {
            int ok = 0;
            Value expr_value = evaluate_expression_text(environment, print_statement->expression_text, &ok);
            if (!ok) {
                fprintf(stderr, "Runtime Error: Failed to evaluate expression '%s'.\n", print_statement->expression_text ? print_statement->expression_text : "");
                return 1;
            }
            print_value(&expr_value);
            value_free(&expr_value);
            printf("\n");
            return 0;
        }

        if (print_statement->value == NULL && !print_statement->has_array_access) {
            return 1;
        }

        if (print_statement->has_array_access) {
            Value value;
            if (!environment_get(environment, print_statement->array_access.name, &value)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", print_statement->array_access.name);
                return 1;
            }

            if (print_statement->value != NULL) {
                resolved = interpolate_template(print_statement->value, environment);
                if (resolved == NULL) {
                    value_free(&value);
                    return 1;
                }
                printf("%s", resolved);
                free(resolved);
            }

            if (value.type != VALUE_ARRAY) {
                fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", print_statement->array_access.name);
                value_free(&value);
                return 1;
            }

            if (print_statement->array_access.is_whole_array) {
                if (print_statement->array_access.whole_array_dimension_count != value.array_value->dimension_count) {
                    fprintf(stderr, "Runtime Error: Array rank is %zu; use %zu empty array access selector%s.\n",
                            value.array_value->dimension_count, value.array_value->dimension_count,
                            value.array_value->dimension_count == 1 ? "" : "s");
                    value_free(&value);
                    return 1;
                }
                print_array_value(value.array_value);
            } else if (print_statement->array_access.is_selection) {
                print_array_selection(value.array_value, &print_statement->array_access);
            } else {
                long *indices = NULL;
                if (!resolve_array_indices(&print_statement->array_access, environment, &indices)) {
                    value_free(&value);
                    return 1;
                }
                print_array_element(value.array_value, indices, print_statement->array_access.index_count);
                free(indices);
            }

            value_free(&value);
            printf("\n");
            return 0;
        }

        if (print_statement->is_variable_reference) {
            if (!environment_get(environment, print_statement->value, &value)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", print_statement->value);
                return 1;
            }
            if (value.type == VALUE_ARRAY) {
                fprintf(stderr, "Syntax Error: Array variable '%s' must be accessed explicitly.\n", print_statement->value);
                value_free(&value);
                return 1;
            }
            print_value(&value);
            value_free(&value);
        } else {
            resolved = interpolate_template(print_statement->value, environment);
            if (resolved == NULL) {
                return 1;
            }
            printf("%s", resolved);
            free(resolved);
        }

        printf("\n");
        return 0;
    }

    if (statement.type == STATEMENT_VARIABLE_DECLARATION) {
        VariableDeclaration *declaration = &statement.as.variable_declaration;

        if (declaration->vars == NULL || declaration->count == 0) {
            return 1;
        }

        for (size_t i = 0; i < declaration->count; ++i) {
            VariableDeclarationSingle *single = &declaration->vars[i];
            Value value;

            if (single->name == NULL) {
                if (single->is_declaration) {
                    fprintf(stderr, "Syntax Error: Expected variable identifier after 'var'.\n");
                } else {
                    fprintf(stderr, "Syntax Error: Expected variable identifier.\n");
                }
                return 1;
            }

            if (single->is_array_element_assignment) {
                Value array_value;
                long *indices = NULL;
                if (!environment_get(environment, single->array_access.name, &array_value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->array_access.name);
                    return 1;
                }
                if (array_value.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", single->array_access.name);
                    value_free(&array_value);
                    return 1;
                }
                if (!resolve_array_indices(&single->array_access, environment, &indices) ||
                    !array_value_set_element(array_value.array_value, indices, single->array_access.index_count, &single->value)) {
                    fprintf(stderr, "Runtime Error: Invalid array assignment.\n");
                    free(indices);
                    value_free(&array_value);
                    return 1;
                }
                free(indices);
                if (!environment_set(environment, single->array_access.name, &array_value)) {
                    fprintf(stderr, "Runtime Error: Unable to update array '%s'.\n", single->array_access.name);
                    value_free(&array_value);
                    return 1;
                }
                value_free(&array_value);
                continue;
            }

            if (single->is_declaration && environment_exists(environment, single->name)) {
                fprintf(stderr, "Runtime Error: Variable '%s' is already declared.\n", single->name);
                return 1;
            }

            if (single->is_input_expression) {
                Value prompt_value = value_create_null();
                char *resolved_prompt = NULL;

                if (single->input_prompt != NULL) {
                    resolved_prompt = interpolate_template(single->input_prompt, environment);
                    if (resolved_prompt == NULL) {
                        return 1;
                    }
                }

                if (single->input_target != NULL) {
                    if (!drift_prompt_and_store(resolved_prompt, single->input_target, &prompt_value)) {
                        free(resolved_prompt);
                        fprintf(stderr, "Runtime Error: Unable to read input for '%s'.\n", single->input_target);
                        return 1;
                    }
                    value = prompt_value;
                } else {
                    if (!drift_prompt_and_store(resolved_prompt, single->name, &prompt_value)) {
                        free(resolved_prompt);
                        fprintf(stderr, "Runtime Error: Unable to read input for '%s'.\n", single->name);
                        return 1;
                    }
                    value = prompt_value;
                }

                free(resolved_prompt);
            } else if (single->has_expression) {
                int expr_ok = 0;
                Value expr_value = evaluate_expression_text(environment, single->expression_text, &expr_ok);
                if (!expr_ok) {
                    fprintf(stderr, "Runtime Error: Failed to evaluate expression '%s'.\n", single->expression_text ? single->expression_text : "");
                    return 1;
                }
                value = expr_value;
            } else if (single->is_assignment && single->has_assignment_operator) {
                Value current_value = value_create_null();
                if (!environment_get(environment, single->name, &current_value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->name);
                    return 1;
                }

                int expr_ok = 0;
                Value rhs = evaluate_expression_text(environment, single->expression_text, &expr_ok);
                if (!expr_ok) {
                    value_free(&current_value);
                    fprintf(stderr, "Runtime Error: Failed to evaluate assignment expression '%s'.\n", single->expression_text ? single->expression_text : "");
                    return 1;
                }

                if (single->assignment_operator == OPERATOR_ADD_ASSIGN) {
                    value = operator_apply(OPERATOR_ADD, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_SUBTRACT_ASSIGN) {
                    value = operator_apply(OPERATOR_SUBTRACT, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_MULTIPLY_ASSIGN) {
                    value = operator_apply(OPERATOR_MULTIPLY, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_DIVIDE_ASSIGN) {
                    value = operator_apply(OPERATOR_DIVIDE, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_MODULO_ASSIGN) {
                    value = operator_apply(OPERATOR_MODULO, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_AND_ASSIGN) {
                    value = operator_apply(OPERATOR_BITWISE_AND, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_OR_ASSIGN) {
                    value = operator_apply(OPERATOR_BITWISE_OR, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_XOR_ASSIGN) {
                    value = operator_apply(OPERATOR_BITWISE_XOR, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_SHIFT_LEFT_ASSIGN) {
                    value = operator_apply(OPERATOR_SHIFT_LEFT, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_SHIFT_RIGHT_ASSIGN) {
                    value = operator_apply(OPERATOR_SHIFT_RIGHT, &current_value, &rhs);
                } else if (single->assignment_operator == OPERATOR_INCREMENT) {
                    value = operator_apply(OPERATOR_INCREMENT, &current_value, NULL);
                } else if (single->assignment_operator == OPERATOR_DECREMENT) {
                    value = operator_apply(OPERATOR_DECREMENT, &current_value, NULL);
                } else {
                    value = rhs;
                }

                value_free(&current_value);
                value_free(&rhs);
            } else if (single->is_assignment && single->is_array_expression) {
                Value source;
                if (!environment_get(environment, single->array_access.name, &source)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->array_access.name);
                    return 1;
                }

                if (source.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", single->array_access.name);
                    value_free(&source);
                    return 1;
                }

                if (!single->array_access.is_selection) {
                    fprintf(stderr, "Syntax Error: Only select() expressions are supported for assignment.\n");
                    value_free(&source);
                    return 1;
                }

                Value *values = (Value *)malloc(single->array_access.selection_count * sizeof(Value));
                if (values == NULL) {
                    fprintf(stderr, "Error: out of memory while building dynamic array\n");
                    value_free(&source);
                    return 1;
                }

                if (single->array_access.selection_tuple_size != source.array_value->dimension_count) {
                    fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", single->array_access.selection_tuple_size, source.array_value->dimension_count);
                    free(values);
                    value_free(&source);
                    return 1;
                }

                for (size_t k = 0; k < single->array_access.selection_count; ++k) {
                    const long *indices = single->array_access.selection_indices + k * single->array_access.selection_tuple_size;
                    int error = 0;
                    const Value *element = array_value_get_element(source.array_value, indices, single->array_access.selection_tuple_size, &error);
                    if (element == NULL) {
                        fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                        for (size_t j = 0; j < k; ++j) {
                            value_free(&values[j]);
                        }
                        free(values);
                        value_free(&source);
                        return 1;
                    }
                    values[k] = value_copy(element);
                }

                ArrayValue *dynamic_array = array_value_create_dynamic_from_values(source.array_value->element_type, values, single->array_access.selection_count);
                for (size_t k = 0; k < single->array_access.selection_count; ++k) {
                    value_free(&values[k]);
                }
                free(values);
                value_free(&source);

                if (dynamic_array == NULL) {
                    fprintf(stderr, "Error: out of memory while creating dynamic array\n");
                    return 1;
                }

                value = value_create_array(dynamic_array);
            } else if (single->is_assignment && single->array_access.name != NULL) {
                Value source;
                if (!environment_get(environment, single->array_access.name, &source)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->array_access.name);
                    return 1;
                }

                if (source.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", single->array_access.name);
                    value_free(&source);
                    return 1;
                }

                if (single->array_access.is_selection) {
                    if (single->array_access.selection_count != 1) {
                        fprintf(stderr, "Runtime Error: Selection yields multiple values; declare an array variable with [].\n");
                        value_free(&source);
                        return 1;
                    }

                    if (single->array_access.selection_tuple_size != source.array_value->dimension_count) {
                        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", single->array_access.selection_tuple_size, source.array_value->dimension_count);
                        value_free(&source);
                        return 1;
                    }

                    int error = 0;
                    const Value *element = array_value_get_element(source.array_value, single->array_access.selection_indices, single->array_access.selection_tuple_size, &error);
                    if (element == NULL) {
                        fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                        value_free(&source);
                        return 1;
                    }
                    value = value_copy(element);
                } else if (single->array_access.is_whole_array) {
                    fprintf(stderr, "Syntax Error: Array variable '%s' must be accessed explicitly.\n", single->array_access.name);
                    value_free(&source);
                    return 1;
                } else {
                    if (single->array_access.index_count == 0) {
                        fprintf(stderr, "Runtime Error: Invalid array access.\n");
                        value_free(&source);
                        return 1;
                    }

                    int error = 0;
                    const Value *element = array_value_get_element(source.array_value, single->array_access.indices, single->array_access.index_count, &error);
                    if (element == NULL) {
                        fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                        value_free(&source);
                        return 1;
                    }
                    value = value_copy(element);
                }

                value_free(&source);
            } else {
                value = single->value;
            }

            if (!environment_set(environment, single->name, &value)) {
                fprintf(stderr, "Runtime Error: Unable to declare variable '%s'.\n", single->name);
                value_free(&value);
                return 1;
            }
        }

        return 0;
    }

    return 1;
}
