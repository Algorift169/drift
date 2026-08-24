/* Repeat execution evaluates bounds and runs the parsed body without changing loop semantics. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/interpreter.h"
#include "drift/repeat.h"

static void strip_whitespace(char *text)
{
    char *read = text;
    char *write = text;

    if (text == NULL) {
        return;
    }

    while (*read != '\0') {
        if (*read != ' ' && *read != '\t' && *read != '\n' && *read != '\r') {
            *write++ = *read;
        }
        read++;
    }
    *write = '\0';
}

static int parse_counter_step_expression(const char *step_expr, const char *counter_name, long *out_step)
{
    char *normalized = NULL;
    size_t name_length = 0;
    char *suffix = NULL;
    char *end_ptr = NULL;
    long number = 0;

    if (step_expr == NULL || counter_name == NULL || out_step == NULL) {
        return 0;
    }

    normalized = drift_duplicate_string(step_expr);
    if (normalized == NULL) {
        return 0;
    }
    strip_whitespace(normalized);
    name_length = strlen(counter_name);

    if (strncmp(normalized, counter_name, name_length) == 0) {
        suffix = normalized + name_length;
        if (strcmp(suffix, "++") == 0) {
            *out_step = 1;
            free(normalized);
            return 1;
        }
        if (strcmp(suffix, "--") == 0) {
            *out_step = -1;
            free(normalized);
            return 1;
        }
        if (suffix[0] == '+' || suffix[0] == '-') {
            errno = 0;
            number = strtol(suffix + 1, &end_ptr, 10);
            if (errno == 0 && end_ptr != suffix + 1 && *end_ptr == '\0') {
                *out_step = (suffix[0] == '+') ? number : -number;
                free(normalized);
                return 1;
            }
        }
    }

    if (strncmp(normalized, "++", 2) == 0 && strcmp(normalized + 2, counter_name) == 0) {
        *out_step = 1;
        free(normalized);
        return 1;
    }
    if (strncmp(normalized, "--", 2) == 0 && strcmp(normalized + 2, counter_name) == 0) {
        *out_step = -1;
        free(normalized);
        return 1;
    }

    if (strncmp(normalized, counter_name, name_length) == 0 && normalized[name_length] == '+' && normalized[name_length + 1] == '+') {
        *out_step = 1;
        free(normalized);
        return 1;
    }
    if (strncmp(normalized, counter_name, name_length) == 0 && normalized[name_length] == '-' && normalized[name_length + 1] == '-') {
        *out_step = -1;
        free(normalized);
        return 1;
    }

    free(normalized);
    return 0;
}

int interpreter_execute_repeat(RepeatStatement *repeat_statement, Environment *environment)
{
    long start = 0;
    long end = 0;
    long step = 1;
    int loop_ok = 0;
    Value loop_value;

    if (repeat_statement == NULL || repeat_statement->counter_name == NULL) {
        fprintf(stderr, "Runtime Error: Invalid repeat statement.\n");
        return 1;
    }

    if (!repeat_statement->has_range) {
        fprintf(stderr, "Runtime Error: Repeat without a range is not supported yet.\n");
        return 1;
    }

    if (repeat_statement->is_infinite) {
        start = 0;
        if (repeat_statement->has_step) {
            char *step_expr = repeat_statement->step_text;
            if (step_expr != NULL && step_expr[0] != '\0') {
                char *normalized = drift_duplicate_string(step_expr);
                if (normalized != NULL) {
                    strip_whitespace(normalized);
                }
                if (normalized != NULL && parse_counter_step_expression(normalized, repeat_statement->counter_name, &step)) {
                    /* handled */
                } else if (normalized != NULL) {
                    Value current = value_create_integer(start);
                    if (!environment_set(environment, repeat_statement->counter_name, &current)) {
                        value_free(&current);
                        free(normalized);
                        fprintf(stderr, "Runtime Error: Unable to initialize loop variable '%s'.\n", repeat_statement->counter_name);
                        return 1;
                    }
                    Value step_eval = interpreter_evaluate_expression(environment, normalized, &loop_ok);
                    value_free(&current);
                    if (!loop_ok || step_eval.type != VALUE_INTEGER) {
                        fprintf(stderr, "Runtime Error: Repeat step value must be an integer.\n");
                        value_free(&step_eval);
                        free(normalized);
                        return 1;
                    }
                    step = step_eval.integer_value - start;
                    value_free(&step_eval);
                    if (step == 0) {
                        fprintf(stderr, "Runtime Error: Repeat step cannot be zero.\n");
                        free(normalized);
                        return 1;
                    }
                }
                free(normalized);
            }
        }

        for (long i = start;; i += step) {
            Value counter_value = value_create_integer(i);
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                int result = interpreter_execute(repeat_statement->body[j], environment);
                if (result != 0) {
                    return result;
                }
            }
        }
        return 0;
    }

    if (repeat_statement->start_text == NULL || repeat_statement->start_text[0] == '\0') {
        start = 0;
    } else {
        loop_value = interpreter_evaluate_expression(environment, repeat_statement->start_text, &loop_ok);
        if (!loop_ok || loop_value.type != VALUE_INTEGER) {
            fprintf(stderr, "Runtime Error: Repeat start value must be an integer.\n");
            value_free(&loop_value);
            return 1;
        }
        start = loop_value.integer_value;
        value_free(&loop_value);
    }

    if (repeat_statement->end_text == NULL || repeat_statement->end_text[0] == '\0') {
        end = start;
    } else {
        loop_value = interpreter_evaluate_expression(environment, repeat_statement->end_text, &loop_ok);
        if (!loop_ok || loop_value.type != VALUE_INTEGER) {
            fprintf(stderr, "Runtime Error: Repeat end value must be an integer.\n");
            value_free(&loop_value);
            return 1;
        }
        end = loop_value.integer_value;
        value_free(&loop_value);
    }

    if (repeat_statement->has_step) {
        char *step_expr = repeat_statement->step_text;
        if (step_expr != NULL && step_expr[0] != '\0') {
            char *normalized = drift_duplicate_string(step_expr);
            if (normalized != NULL) {
                strip_whitespace(normalized);
            }

            if (normalized != NULL && parse_counter_step_expression(normalized, repeat_statement->counter_name, &step)) {
                /* handled */
            } else if (normalized != NULL) {
                Value current = value_create_integer(start);
                if (!environment_set(environment, repeat_statement->counter_name, &current)) {
                    value_free(&current);
                    free(normalized);
                    fprintf(stderr, "Runtime Error: Unable to initialize loop variable '%s'.\n", repeat_statement->counter_name);
                    return 1;
                }
                Value step_eval = interpreter_evaluate_expression(environment, normalized, &loop_ok);
                value_free(&current);
                if (!loop_ok || step_eval.type != VALUE_INTEGER) {
                    fprintf(stderr, "Runtime Error: Repeat step value must be an integer.\n");
                    value_free(&step_eval);
                    free(normalized);
                    return 1;
                }
                step = step_eval.integer_value - start;
                value_free(&step_eval);
                if (step == 0) {
                    fprintf(stderr, "Runtime Error: Repeat step cannot be zero.\n");
                    free(normalized);
                    return 1;
                }
            }
            free(normalized);
        }
    } else if (repeat_statement->is_exclusive_upper) {
        step = (start <= end) ? 1 : -1;
    } else if (repeat_statement->is_exclusive_lower) {
        step = (start >= end) ? -1 : 1;
    } else if (start <= end) {
        step = 1;
    } else {
        step = -1;
    }

    if (step == 0) {
        fprintf(stderr, "Runtime Error: Repeat step cannot be zero.\n");
        return 1;
    }

    if (repeat_statement->is_exclusive_upper) {
        for (long i = start; (step > 0) ? (i < end) : (i > end); i += step) {
            Value counter_value = value_create_integer(i);
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                int result = interpreter_execute(repeat_statement->body[j], environment);
                if (result != 0) {
                    return result;
                }
            }
        }
    } else if (repeat_statement->is_exclusive_lower) {
        for (long i = start; i > end; i += step) {
            Value counter_value = value_create_integer(i);
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                int result = interpreter_execute(repeat_statement->body[j], environment);
                if (result != 0) {
                    return result;
                }
            }
        }
    } else {
        for (long i = start; (step > 0) ? (i <= end) : (i >= end); i += step) {
            Value counter_value = value_create_integer(i);
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                int result = interpreter_execute(repeat_statement->body[j], environment);
                if (result != 0) {
                    return result;
                }
            }
        }
    }

    return 0;
}
