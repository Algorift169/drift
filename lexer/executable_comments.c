#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drift/executable_comments.h"

/* Helper to check if a trimmed line matches @exc or .exc */
static int is_exc_marker(const char *line, const char *marker)
{
    if (line == NULL || marker == NULL) {
        return 0;
    }
    return strcmp(line, marker) == 0;
}

/* Trim leading/trailing whitespace from a line */
static char *trim_line(const char *line, size_t line_len)
{
    const char *start = line;
    const char *end = line + line_len;

    while (start < end && (*start == ' ' || *start == '\t')) {
        start++;
    }

    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) {
        end--;
    }

    size_t trimmed_len = end - start;
    char *result = (char *)malloc(trimmed_len + 1);
    if (result) {
        memcpy(result, start, trimmed_len);
        result[trimmed_len] = '\0';
    }
    return result;
}

/* Check if current position starts a block comment closing sequence */
static int check_block_end(const char *source, size_t pos, size_t len)
{
    if (pos + 1 < len && source[pos] == '*' && source[pos + 1] == '/') {
        return 1;
    }
    return 0;
}

char *extract_executable_from_exc_blocks(const char *source)
{
    size_t source_len;
    char *result;
    size_t result_idx = 0;
    size_t i = 0;
    int in_block_comment = 0;
    int in_exc_block = 0;

    if (source == NULL) {
        result = (char *)malloc(1);
        if (result) result[0] = '\0';
        return result;
    }

    source_len = strlen(source);
    result = (char *)malloc(source_len + 1);
    if (result == NULL) {
        return NULL;
    }

    while (i < source_len) {
        /* Look for start of block comment */
        if (!in_block_comment && i + 1 < source_len && source[i] == '/' && source[i + 1] == '*') {
            in_block_comment = 1;
            i += 2;
            continue;
        }

        /* If not in block comment, copy character as-is */
        if (!in_block_comment) {
            result[result_idx++] = source[i++];
            continue;
        }

        /* In block comment: look for lines and markers */
        size_t line_start = i;
        size_t line_end = i;

        /* Find end of line or block end */
        while (line_end < source_len && source[line_end] != '\n') {
            if (check_block_end(source, line_end, source_len)) {
                break;
            }
            line_end++;
        }

        /* Check if we hit a block comment end marker */
        if (line_end < source_len && source[line_end] != '\n') {
            /* This is the end of block comment */
            char *trimmed = trim_line(source + line_start, line_end - line_start);
            if (trimmed) {
                if (is_exc_marker(trimmed, ".exc")) {
                    in_exc_block = 0;
                }
                free(trimmed);
            }
            in_block_comment = 0;
            i = line_end + 2;
            continue;
        }

        /* Trim the line to check for @exc and .exc */
        char *trimmed = trim_line(source + line_start, line_end - line_start);

        if (trimmed) {
            if (is_exc_marker(trimmed, "@exc")) {
                in_exc_block = 1;
            } else if (is_exc_marker(trimmed, ".exc")) {
                in_exc_block = 0;
            } else if (in_exc_block && trimmed[0] != '\0') {
                /* This is executable code */
                if (result_idx > 0 && result[result_idx - 1] != '\n') {
                    result[result_idx++] = '\n';
                }
                strcpy(&result[result_idx], trimmed);
                result_idx += strlen(trimmed);
                result[result_idx++] = '\n';
            }
            free(trimmed);
        }

        /* Move to next line */
        i = line_end;
        if (i < source_len && source[i] == '\n') {
            i++;
        }
    }

    result[result_idx] = '\0';
    return result;
}
