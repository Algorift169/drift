#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/interpreter.h"

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
        default:
            break;
    }
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
            }
            value_free(&value);

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

        if (print_statement->value == NULL) {
            return 1;
        }

        if (print_statement->is_variable_reference) {
            if (!environment_get(environment, print_statement->value, &value)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", print_statement->value);
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
        Value value = declaration->value;

        if (declaration->name == NULL) {
            fprintf(stderr, "Syntax Error: Expected variable identifier after 'var'.\n");
            return 1;
        }

        if (environment_exists(environment, declaration->name)) {
            fprintf(stderr, "Runtime Error: Variable '%s' is already declared.\n", declaration->name);
            return 1;
        }

        if (!environment_set(environment, declaration->name, &value)) {
            fprintf(stderr, "Runtime Error: Unable to declare variable '%s'.\n", declaration->name);
            return 1;
        }

        return 0;
    }

    return 1;
}
