#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/input.h"
#include "drift/value.h"

static void trim_in_place(char *text)
{
    char *start;
    char *end;

    if (text == NULL) {
        return;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1U);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';
}

char *drift_read_line_from_stdin(const char *prompt)
{
    char buffer[4096];
    char *line = NULL;
    size_t length;

    if (prompt != NULL && prompt[0] != '\0') {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }

    line = (char *)malloc(length + 1U);
    if (line == NULL) {
        fprintf(stderr, "Error: out of memory while reading input\n");
        return NULL;
    }

    strcpy(line, buffer);
    return line;
}

static char **s_input_tokens = NULL;
static size_t s_input_count = 0;
static size_t s_input_index = 0;

static void clear_input_buffer(void)
{
    if (s_input_tokens != NULL) {
        for (size_t i = 0; i < s_input_count; ++i) {
            free(s_input_tokens[i]);
        }
        free(s_input_tokens);
        s_input_tokens = NULL;
    }
    s_input_count = 0;
    s_input_index = 0;
}

static char **tokenize_input_line(const char *line, size_t *out_count)
{
    char **tokens = NULL;
    size_t capacity = 8;
    size_t count = 0;
    size_t i = 0;
    size_t len = line ? strlen(line) : 0;

    if (out_count != NULL) {
        *out_count = 0;
    }

    if (line == NULL || len == 0) {
        return NULL;
    }

    tokens = (char **)malloc(capacity * sizeof(char *));
    if (tokens == NULL) {
        return NULL;
    }

    while (i < len) {
        while (i < len && isspace((unsigned char)line[i])) {
            i++;
        }
        if (i >= len) {
            break;
        }

        size_t start = i;
        char quote = '\0';
        if (line[i] == '"' || line[i] == '\'') {
            quote = line[i];
            i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) {
                    i++;
                }
                i++;
            }
            if (i < len && line[i] == quote) {
                i++;
            }
        } else {
            while (i < len && !isspace((unsigned char)line[i])) {
                i++;
            }
        }

        size_t token_len = i - start;
        char *token = (char *)malloc(token_len + 1U);
        if (token != NULL) {
            memcpy(token, line + start, token_len);
            token[token_len] = '\0';

            if (count >= capacity) {
                capacity *= 2;
                char **new_tokens = (char **)realloc(tokens, capacity * sizeof(char *));
                if (new_tokens == NULL) {
                    free(token);
                    break;
                }
                tokens = new_tokens;
            }
            tokens[count++] = token;
        }
    }

    if (out_count != NULL) {
        *out_count = count;
    }
    return tokens;
}

int drift_parse_input_value(const char *text, Value *out_value)
{
    char *trimmed;
    char *end = NULL;
    long integer_value;
    double float_value;

    if (text == NULL || out_value == NULL) {
        return 0;
    }

    trimmed = (char *)malloc(strlen(text) + 1U);
    if (trimmed == NULL) {
        return 0;
    }
    strcpy(trimmed, text);
    trim_in_place(trimmed);

    if ((trimmed[0] == '"' || trimmed[0] == '\'') && strlen(trimmed) >= 2 && trimmed[strlen(trimmed) - 1] == trimmed[0]) {
        size_t len = strlen(trimmed);
        trimmed[len - 1] = '\0';
        *out_value = value_create_string(trimmed + 1);
        free(trimmed);
        return 1;
    }

    if (strcmp(trimmed, "true") == 0 || strcmp(trimmed, "TRUE") == 0) {
        free(trimmed);
        *out_value = value_create_boolean(1);
        return 1;
    }

    if (strcmp(trimmed, "false") == 0 || strcmp(trimmed, "FALSE") == 0) {
        free(trimmed);
        *out_value = value_create_boolean(0);
        return 1;
    }

    if (strcmp(trimmed, "null") == 0 || strcmp(trimmed, "NULL") == 0) {
        free(trimmed);
        *out_value = value_create_null();
        return 1;
    }

    if (strcmp(trimmed, "inf") == 0 || strcmp(trimmed, "INF") == 0) {
        free(trimmed);
        *out_value = value_create_infinity();
        return 1;
    }

    integer_value = strtol(trimmed, &end, 10);
    if (end != trimmed && (*end == '\0' || isspace((unsigned char)*end))) {
        free(trimmed);
        *out_value = value_create_integer(integer_value);
        return 1;
    }

    float_value = strtod(trimmed, &end);
    if (end != trimmed && (*end == '\0' || isspace((unsigned char)*end))) {
        free(trimmed);
        *out_value = value_create_float(float_value);
        return 1;
    }

    free(trimmed);
    *out_value = value_create_string(text);
    return 1;
}

int drift_prompt_and_store(const char *prompt, const char *target_name, Value *out_value)
{
    if (target_name == NULL || out_value == NULL) {
        return 0;
    }

    if (prompt != NULL && prompt[0] != '\0') {
        clear_input_buffer();
    }

    if (s_input_tokens != NULL && s_input_index < s_input_count) {
        char *token = s_input_tokens[s_input_index++];
        int ok = drift_parse_input_value(token, out_value);
        if (s_input_index >= s_input_count) {
            clear_input_buffer();
        }
        return ok;
    }

    clear_input_buffer();

    const char *current_prompt = prompt;
    while (1) {
        char *line = drift_read_line_from_stdin(current_prompt);
        if (line == NULL) {
            *out_value = value_create_string("");
            return 1;
        }

        size_t count = 0;
        char **tokens = tokenize_input_line(line, &count);
        free(line);

        if (tokens != NULL && count > 0) {
            s_input_tokens = tokens;
            s_input_count = count;
            s_input_index = 0;

            char *token = s_input_tokens[s_input_index++];
            int ok = drift_parse_input_value(token, out_value);
            if (s_input_index >= s_input_count) {
                clear_input_buffer();
            }
            return ok;
        }

        if (tokens != NULL) {
            free(tokens);
        }
        current_prompt = NULL;
    }
}
