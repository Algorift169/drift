#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/array_value.h"
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

int interpreter_execute(Statement statement, Environment *environment)
{
    if (environment == NULL) {
        fprintf(stderr, "Runtime Error: missing execution environment.\n");
        return 1;
    }

    if (statement.type == STATEMENT_PRINT) {
        PrintStatement *print_statement = &statement.as.print_statement;
        Value value;
        char *resolved = NULL;

        if (print_statement->value == NULL && !print_statement->has_array_access) {
            return 1;
        }

        if (print_statement->has_array_access) {
            int error = 0;
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
                print_array_element(value.array_value, print_statement->array_access.indices, print_statement->array_access.index_count);
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
        Value value;

        if (declaration->name == NULL) {
            fprintf(stderr, "Syntax Error: Expected variable identifier after 'var'.\n");
            return 1;
        }

        if (declaration->is_declaration && environment_exists(environment, declaration->name)) {
            fprintf(stderr, "Runtime Error: Variable '%s' is already declared.\n", declaration->name);
            return 1;
        }

        if (declaration->is_assignment && declaration->is_array_expression) {
            Value source;
            if (!environment_get(environment, declaration->array_access.name, &source)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", declaration->array_access.name);
                return 1;
            }

            if (source.type != VALUE_ARRAY) {
                fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", declaration->array_access.name);
                value_free(&source);
                return 1;
            }

            if (!declaration->array_access.is_selection) {
                fprintf(stderr, "Syntax Error: Only select() expressions are supported for assignment.\n");
                value_free(&source);
                return 1;
            }

            Value *values = (Value *)malloc(declaration->array_access.selection_count * sizeof(Value));
            if (values == NULL) {
                fprintf(stderr, "Error: out of memory while building dynamic array\n");
                value_free(&source);
                return 1;
            }

            if (declaration->array_access.selection_tuple_size != source.array_value->dimension_count) {
                fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", declaration->array_access.selection_tuple_size, source.array_value->dimension_count);
                free(values);
                value_free(&source);
                return 1;
            }

            for (size_t i = 0; i < declaration->array_access.selection_count; ++i) {
                const long *indices = declaration->array_access.selection_indices + i * declaration->array_access.selection_tuple_size;
                int error = 0;
                const Value *element = array_value_get_element(source.array_value, indices, declaration->array_access.selection_tuple_size, &error);
                if (element == NULL) {
                    fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                    for (size_t j = 0; j < i; ++j) {
                        value_free(&values[j]);
                    }
                    free(values);
                    value_free(&source);
                    return 1;
                }
                values[i] = value_copy(element);
            }

            ArrayValue *dynamic_array = array_value_create_dynamic_from_values(source.array_value->element_type, values, declaration->array_access.selection_count);
            for (size_t i = 0; i < declaration->array_access.selection_count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            value_free(&source);

            if (dynamic_array == NULL) {
                fprintf(stderr, "Error: out of memory while creating dynamic array\n");
                return 1;
            }

            value = value_create_array(dynamic_array);
        } else if (declaration->is_assignment && declaration->array_access.name != NULL) {
            Value source;
            if (!environment_get(environment, declaration->array_access.name, &source)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", declaration->array_access.name);
                return 1;
            }

            if (source.type != VALUE_ARRAY) {
                fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", declaration->array_access.name);
                value_free(&source);
                return 1;
            }

            if (declaration->array_access.is_selection) {
                if (declaration->array_access.selection_count != 1) {
                    fprintf(stderr, "Runtime Error: Selection yields multiple values; declare an array variable with [].\n");
                    value_free(&source);
                    return 1;
                }

                if (declaration->array_access.selection_tuple_size != source.array_value->dimension_count) {
                    fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", declaration->array_access.selection_tuple_size, source.array_value->dimension_count);
                    value_free(&source);
                    return 1;
                }

                int error = 0;
                const Value *element = array_value_get_element(source.array_value, declaration->array_access.selection_indices, declaration->array_access.selection_tuple_size, &error);
                if (element == NULL) {
                    fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                    value_free(&source);
                    return 1;
                }
                value = value_copy(element);
            } else if (declaration->array_access.is_whole_array) {
                fprintf(stderr, "Syntax Error: Array variable '%s' must be accessed explicitly.\n", declaration->array_access.name);
                value_free(&source);
                return 1;
            } else {
                if (declaration->array_access.index_count == 0) {
                    fprintf(stderr, "Runtime Error: Invalid array access.\n");
                    value_free(&source);
                    return 1;
                }

                int error = 0;
                const Value *element = array_value_get_element(source.array_value, declaration->array_access.indices, declaration->array_access.index_count, &error);
                if (element == NULL) {
                    fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                    value_free(&source);
                    return 1;
                }
                value = value_copy(element);
            }

            value_free(&source);
        } else {
            value = declaration->value;
        }

        if (!environment_set(environment, declaration->name, &value)) {
            fprintf(stderr, "Runtime Error: Unable to declare variable '%s'.\n", declaration->name);
            value_free(&value);
            return 1;
        }

        return 0;
    }

    return 1;
}
