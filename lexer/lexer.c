#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/comments.h"
#include "drift/lexer.h"

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static int is_newline(char c)
{
    return c == '\n';
}

static int is_identifier_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_identifier_part(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

static Token make_token(TokenType type, char *value)
{
    Token token;
    token.type = type;
    token.value = value;
    return token;
}

Lexer lexer_create(const char *source)
{
    Lexer lexer;
    lexer.source = source;
    lexer.index = 0;
    lexer.length = 0;
    lexer.in_block_comment = 0;
    lexer.in_block_comment_code = 0;

    if (source != NULL) {
        while (source[lexer.length] != '\0') {
            lexer.length++;
        }
    }

    return lexer;
}

static void skip_whitespace(Lexer *lexer)
{
    while (lexer->index < lexer->length && is_space(lexer->source[lexer->index])) {
        lexer->index++;
    }
}

static Token read_identifier(Lexer *lexer)
{
    size_t start = lexer->index;
    size_t length = 0;
    char *value;

    if (!is_identifier_start(lexer->source[lexer->index])) {
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    while (lexer->index < lexer->length && is_identifier_part(lexer->source[lexer->index])) {
        lexer->index++;
        length++;
    }

    value = (char *)malloc(length + 1U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading identifier\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    for (size_t i = 0; i < length; ++i) {
        value[i] = lexer->source[start + i];
    }
    value[length] = '\0';

    if (strcmp(value, "say") == 0) {
        free(value);
        return make_token(TOKEN_SAY, drift_duplicate_string("say"));
    }

    if (strcmp(value, "ask") == 0) {
        free(value);
        return make_token(TOKEN_ASK, drift_duplicate_string("ask"));
    }

    if (strcmp(value, "var") == 0) {
        free(value);
        return make_token(TOKEN_VAR, drift_duplicate_string("var"));
    }

    if (strcmp(value, "true") == 0 || strcmp(value, "TRUE") == 0) {
        free(value);
        return make_token(TOKEN_TRUE, drift_duplicate_string("true"));
    }

    if (strcmp(value, "false") == 0 || strcmp(value, "FALSE") == 0) {
        free(value);
        return make_token(TOKEN_FALSE, drift_duplicate_string("false"));
    }

    if (strcmp(value, "NULL") == 0 || strcmp(value, "null") == 0) {
        free(value);
        return make_token(TOKEN_NULL, drift_duplicate_string("NULL"));
    }

    if (strcmp(value, "INF") == 0 || strcmp(value, "inf") == 0) {
        free(value);
        return make_token(TOKEN_INFINITY, drift_duplicate_string("INF"));
    }

    return make_token(TOKEN_IDENTIFIER, value);
}

static Token read_number(Lexer *lexer)
{
    size_t start = lexer->index;
    int has_decimal = 0;
    char *value;
    size_t length = 0;

    while (lexer->index < lexer->length && (isdigit((unsigned char)lexer->source[lexer->index]) || lexer->source[lexer->index] == '.')) {
        if (lexer->source[lexer->index] == '.') {
            if (has_decimal) {
                fprintf(stderr, "Syntax Error: Invalid number literal.\n");
                return make_token(TOKEN_UNKNOWN, NULL);
            }
            has_decimal = 1;
        }
        lexer->index++;
        length++;
    }

    value = (char *)malloc(length + 1U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading number\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    for (size_t i = 0; i < length; ++i) {
        value[i] = lexer->source[start + i];
    }
    value[length] = '\0';

    if (has_decimal) {
        return make_token(TOKEN_FLOAT, value);
    }

    return make_token(TOKEN_INTEGER, value);
}

static Token read_single_quoted_value(Lexer *lexer)
{
    size_t start = lexer->index;
    size_t length = 0;
    char *value;
    size_t i;
    size_t out_index = 0;

    if (lexer->index >= lexer->length || lexer->source[lexer->index] != '\'') {
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    lexer->index++;
    while (lexer->index < lexer->length && lexer->source[lexer->index] != '\'') {
        if (lexer->source[lexer->index] == '\n') {
            fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
            return make_token(TOKEN_UNKNOWN, NULL);
        }
        lexer->index++;
        length++;
    }

    if (lexer->index >= lexer->length) {
        fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    value = (char *)malloc(length + 3U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading single-quoted value\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    value[0] = '\'';
    i = start + 1;
    while (i < lexer->index) {
        value[out_index + 1] = lexer->source[i];
        out_index++;
        i++;
    }
    value[out_index + 1] = '\'';
    value[out_index + 2] = '\0';

    lexer->index++;
    return make_token(TOKEN_STRING, value);
}

static Token read_string(Lexer *lexer)
{
    size_t start = lexer->index;
    size_t length = 0;
    char *value;
    size_t i;
    size_t out_index = 0;

    if (lexer->index >= lexer->length || lexer->source[lexer->index] != '"') {
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    lexer->index++;
    while (lexer->index < lexer->length && lexer->source[lexer->index] != '"') {
        if (lexer->source[lexer->index] == '\n') {
            fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
            return make_token(TOKEN_UNKNOWN, NULL);
        }

        if (lexer->source[lexer->index] == '\\' && lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == 'n') {
            length++;
            lexer->index += 2;
            continue;
        }

        lexer->index++;
        length++;
    }

    if (lexer->index >= lexer->length) {
        fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    value = (char *)malloc(length + 1U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading string\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    i = start + 1;
    while (i < lexer->index) {
        if (lexer->source[i] == '\\' && i + 1 < lexer->index && lexer->source[i + 1] == 'n') {
            value[out_index++] = '\n';
            i += 2;
        } else {
            value[out_index++] = lexer->source[i];
            i++;
        }
    }
    value[out_index] = '\0';

    lexer->index++;
    return make_token(TOKEN_STRING, value);
}

Token *lexer_scan_all(Lexer *lexer, size_t *token_count)
{
    Token *tokens = NULL;
    size_t capacity = 16;
    size_t count = 0;

    if (token_count != NULL) {
        *token_count = 0;
    }

    tokens = (Token *)malloc(capacity * sizeof(Token));
    if (tokens == NULL) {
        fprintf(stderr, "Error: out of memory while allocating tokens\n");
        return NULL;
    }

    while (lexer->index < lexer->length) {
        skip_whitespace(lexer);

        int comment_res = lexer_skip_comments(lexer);
        if (comment_res < 0) {
            token_free_array(tokens, count);
            return NULL;
        }
        if (comment_res > 0) {
            continue;
        }

        if (lexer->index >= lexer->length) {
            break;
        }

        if (is_newline(lexer->source[lexer->index])) {
            tokens[count++] = make_token(TOKEN_NEWLINE, NULL);
            lexer->index++;
            if (count >= capacity) {
                Token *new_tokens;
                capacity *= 2;
                new_tokens = (Token *)realloc(tokens, capacity * sizeof(Token));
                if (new_tokens == NULL) {
                    fprintf(stderr, "Error: out of memory while growing token array\n");
                    free(tokens);
                    return NULL;
                }
                tokens = new_tokens;
            }
            continue;
        }

        if (lexer->source[lexer->index] == '=') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_EQUAL_EQUAL, drift_duplicate_string("=="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_EQUAL, drift_duplicate_string("="));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '+') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '+') {
                tokens[count++] = make_token(TOKEN_PLUS_PLUS, drift_duplicate_string("++"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_PLUS_EQUAL, drift_duplicate_string("+="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_PLUS, drift_duplicate_string("+"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '-') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '-') {
                tokens[count++] = make_token(TOKEN_MINUS_MINUS, drift_duplicate_string("--"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_MINUS_EQUAL, drift_duplicate_string("-="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_MINUS, drift_duplicate_string("-"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '*') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_STAR_EQUAL, drift_duplicate_string("*="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_STAR, drift_duplicate_string("*"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '/') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_SLASH_EQUAL, drift_duplicate_string("/="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_SLASH, drift_duplicate_string("/"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '%') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_PERCENT_EQUAL, drift_duplicate_string("%="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_PERCENT, drift_duplicate_string("%"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '!') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_NOT_EQUAL, drift_duplicate_string("!="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_BANG, drift_duplicate_string("!"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '<') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_LEFT_EQUAL, drift_duplicate_string("<<="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_LESS_EQUAL, drift_duplicate_string("<="));
                    lexer->index += 2;
                }
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '<') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_LEFT_EQUAL, drift_duplicate_string("<<="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_SHIFT_LEFT, drift_duplicate_string("<<"));
                    lexer->index += 2;
                }
            } else {
                tokens[count++] = make_token(TOKEN_LESS, drift_duplicate_string("<"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '>') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_RIGHT_EQUAL, drift_duplicate_string(">>="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_GREATER_EQUAL, drift_duplicate_string(">="));
                    lexer->index += 2;
                }
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '>') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_RIGHT_EQUAL, drift_duplicate_string(">>="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_SHIFT_RIGHT, drift_duplicate_string(">>"));
                    lexer->index += 2;
                }
            } else {
                tokens[count++] = make_token(TOKEN_GREATER, drift_duplicate_string(">"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '&') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '&') {
                tokens[count++] = make_token(TOKEN_AND_AND, drift_duplicate_string("&&"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_AMPERSAND_EQUAL, drift_duplicate_string("&="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_AMPERSAND, drift_duplicate_string("&"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '|') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '|') {
                tokens[count++] = make_token(TOKEN_OR_OR, drift_duplicate_string("||"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_PIPE_EQUAL, drift_duplicate_string("|="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_PIPE, drift_duplicate_string("|"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '^') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_CARET_EQUAL, drift_duplicate_string("^="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_CARET, drift_duplicate_string("^"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '~') {
            tokens[count++] = make_token(TOKEN_TILDA, drift_duplicate_string("~"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '.') {
            if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 1] == '.' && lexer->source[lexer->index + 2] == '.') {
                tokens[count++] = make_token(TOKEN_RANGE, drift_duplicate_string("..."));
                lexer->index += 3;
            } else {
                tokens[count++] = make_token(TOKEN_DOT, drift_duplicate_string("."));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '@') {
            tokens[count++] = make_token(TOKEN_AT, drift_duplicate_string("@"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '{') {
            tokens[count++] = make_token(TOKEN_LEFT_BRACE, drift_duplicate_string("{"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '}') {
            tokens[count++] = make_token(TOKEN_RIGHT_BRACE, drift_duplicate_string("}"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '[') {
            tokens[count++] = make_token(TOKEN_LEFT_BRACKET, drift_duplicate_string("["));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ']') {
            tokens[count++] = make_token(TOKEN_RIGHT_BRACKET, drift_duplicate_string("]"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '(') {
            tokens[count++] = make_token(TOKEN_LEFT_PAREN, drift_duplicate_string("("));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ')') {
            tokens[count++] = make_token(TOKEN_RIGHT_PAREN, drift_duplicate_string(")"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ',') {
            tokens[count++] = make_token(TOKEN_COMMA, drift_duplicate_string(","));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ';') {
            tokens[count++] = make_token(TOKEN_SEMICOLON, drift_duplicate_string(";"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '\'') {
            Token token = read_single_quoted_value(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else if (lexer->source[lexer->index] == '"') {
            Token token = read_string(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else if (isdigit((unsigned char)lexer->source[lexer->index])) {
            Token token = read_number(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else if (is_identifier_start(lexer->source[lexer->index])) {
            Token token = read_identifier(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                fprintf(stderr, "Syntax Error: Invalid identifier '%c'\n", lexer->source[lexer->index]);
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else {
            fprintf(stderr, "Syntax Error: Unknown token '%c'\n", lexer->source[lexer->index]);
            token_free_array(tokens, count);
            return NULL;
        }

        if (count >= capacity) {
            Token *new_tokens;
            capacity *= 2;
            new_tokens = (Token *)realloc(tokens, capacity * sizeof(Token));
            if (new_tokens == NULL) {
                fprintf(stderr, "Error: out of memory while growing token array\n");
                token_free_array(tokens, count);
                return NULL;
            }
            tokens = new_tokens;
        }
    }

    if (lexer->in_block_comment) {
        fprintf(stderr, "Syntax Error: Unterminated block comment.\n");
        token_free_array(tokens, count);
        return NULL;
    }

    tokens[count++] = make_token(TOKEN_EOF, NULL);

    if (token_count != NULL) {
        *token_count = count;
    }

    return tokens;
}
