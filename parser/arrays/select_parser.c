#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/parser.h"
#include "drift/value.h"

static Token *parser_peek(Parser *parser);
static Token *parser_advance(Parser *parser);

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

static int parse_integer_tuple(Parser *parser, long **out_indices, size_t *out_count)
{
    size_t count = 0;
    long *indices = NULL;

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Expected integer index in select() arguments.\n");
            free(indices);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        if (token->type != TOKEN_INTEGER) {
            fprintf(stderr, "Syntax Error: Expected integer index in select() arguments.\n");
            free(indices);
            return 0;
        }

        long index = 0;
        char *end = NULL;
        index = strtol(token->value, &end, 10);
        (void)end;

        parser_advance(parser);
        long *new_indices = (long *)realloc(indices, (count + 1U) * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() indices\n");
            free(indices);
            return 0;
        }
        indices = new_indices;
        indices[count++] = index;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (token != NULL && token->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or ')' in select() arguments.\n");
        free(indices);
        return 0;
    }

    *out_indices = indices;
    *out_count = count;
    return 1;
}

static int parse_tuple_group(Parser *parser, long **out_indices, size_t *out_count, size_t *out_tuple_size)
{
    size_t tuple_count = 0;
    size_t tuple_size = 0;
    long *indices = NULL;

    if (!parser_expect(parser, TOKEN_LEFT_PAREN, "Syntax Error: Expected '(' in select() arguments.")) {
        return 0;
    }

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated tuple in select() arguments.\n");
            free(indices);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_PAREN) {
            parser_advance(parser);
            break;
        }

        long *child_indices = NULL;
        size_t child_tuple_count = 0;
        size_t child_tuple_size = 0;

        if (token->type == TOKEN_LEFT_PAREN) {
            if (!parse_tuple_group(parser, &child_indices, &child_tuple_count, &child_tuple_size)) {
                free(indices);
                return 0;
            }
        } else {
            if (!parse_integer_tuple(parser, &child_indices, &child_tuple_size)) {
                free(indices);
                return 0;
            }
            child_tuple_count = 1U;
        }

        if (child_tuple_size == 0) {
            fprintf(stderr, "Syntax Error: Empty tuple in select() arguments.\n");
            free(indices);
            free(child_indices);
            return 0;
        }

        if (tuple_size == 0) {
            tuple_size = child_tuple_size;
        } else if (child_tuple_size != tuple_size) {
            fprintf(stderr, "Syntax Error: Inconsistent select() tuple length.\n");
            free(indices);
            free(child_indices);
            return 0;
        }

        long *new_indices = (long *)realloc(indices, (tuple_count + child_tuple_count) * tuple_size * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(indices);
            free(child_indices);
            return 0;
        }
        indices = new_indices;
        memcpy(indices + tuple_count * tuple_size, child_indices, child_tuple_count * tuple_size * sizeof(long));
        tuple_count += child_tuple_count;
        free(child_indices);

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        break;
    }

    if (!parser_expect(parser, TOKEN_RIGHT_PAREN, "Syntax Error: Expected ')' to close select() tuple.")) {
        free(indices);
        return 0;
    }

    *out_indices = indices;
    *out_count = tuple_count;
    *out_tuple_size = tuple_size;
    return 1;
}

int parse_select_access(Parser *parser, ArrayAccess *access)
{
    if (access == NULL) {
        return 0;
    }

    if (!parser_expect(parser, TOKEN_DOT, "Syntax Error: Expected '.' before select().")) {
        return 0;
    }

    Token *token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER || strcmp(token->value, "select") != 0) {
        fprintf(stderr, "Syntax Error: Expected select() after array name.\n");
        return 0;
    }
    parser_advance(parser);

    if (!parser_expect(parser, TOKEN_LEFT_PAREN, "Syntax Error: Expected '(' after select.")) {
        return 0;
    }

    size_t tuple_count = 0;
    size_t tuple_size = 0;
    long *indices = NULL;
    int *breaks = NULL;

    Token *first_argument = parser_peek(parser);
    if (first_argument != NULL && first_argument->type == TOKEN_INTEGER) {
        if (!parse_integer_tuple(parser, &indices, &tuple_size)) {
            return 0;
        }
        tuple_count = 1;
        breaks = (int *)calloc(1, sizeof(int));
        if (breaks == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(indices);
            return 0;
        }
    } else while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated select() arguments.\n");
            free(indices);
            free(breaks);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        long *tuple = NULL;
        size_t tuple_len = 0;
        size_t child_tuple_size = 0;
        if (!parse_tuple_group(parser, &tuple, &tuple_len, &child_tuple_size)) {
            free(indices);
            free(breaks);
            return 0;
        }

        if (tuple_size == 0) {
            tuple_size = child_tuple_size;
        } else if (child_tuple_size != tuple_size) {
            fprintf(stderr, "Syntax Error: Inconsistent select() tuple length.\n");
            free(tuple);
            free(indices);
            free(breaks);
            return 0;
        }

        long *new_indices = (long *)realloc(indices, (tuple_count + tuple_len) * tuple_size * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(tuple);
            free(indices);
            free(breaks);
            return 0;
        }
        indices = new_indices;
        memcpy(indices + tuple_count * tuple_size, tuple, tuple_len * tuple_size * sizeof(long));
        free(tuple);

        int *new_breaks = (int *)realloc(breaks, (tuple_count + tuple_len) * sizeof(int));
        if (new_breaks == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(indices);
            free(breaks);
            return 0;
        }
        breaks = new_breaks;
        for (size_t i = 0; i < tuple_len; ++i) {
            breaks[tuple_count + i] = 0;
        }
        tuple_count += tuple_len;

        Token *separator = parser_peek(parser);
        if (separator == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated select() arguments.\n");
            free(indices);
            free(breaks);
            return 0;
        }

        if (separator->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (separator->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or ')' in select() arguments.\n");
        free(indices);
        free(breaks);
        return 0;
    }

    if (!parser_expect(parser, TOKEN_RIGHT_PAREN, "Syntax Error: Expected ')' after select() arguments.")) {
        free(indices);
        free(breaks);
        return 0;
    }

    access->is_selection = 1;
    access->selection_count = tuple_count;
    access->selection_tuple_size = tuple_size;
    access->selection_indices = indices;
    access->selection_breaks = breaks;
    return 1;
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

    Token *token = &parser->tokens[parser->index];
    parser->index++;
    return token;
}
