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
    char *line = NULL;
    int ok = 0;

    if (target_name == NULL || out_value == NULL) {
        return 0;
    }

    line = drift_read_line_from_stdin(prompt);
    if (line == NULL) {
        return 0;
    }

    ok = drift_parse_input_value(line, out_value);
    free(line);
    return ok;
}
