#include <stdio.h>
#include <string.h>
#include "drift/comments.h"

int is_block_comment_open(const char *source)
{
    if (source == NULL) {
        return 0;
    }

    int in_block = 0;
    size_t i = 0;
    while (source[i] != '\0') {
        if (!in_block && (source[i] == '\'' || source[i] == '"')) {
            char quote = source[i];
            i++;
            while (source[i] != '\0' && source[i] != quote) {
                if (source[i] == '\\' && source[i + 1] != '\0') {
                    i += 2;
                } else {
                    i++;
                }
            }
            if (source[i] == quote) {
                i++;
            }
            continue;
        }

        if (!in_block && source[i] == '/' && source[i + 1] == '/') {
            i += 2;
            while (source[i] != '\0' && source[i] != '\n') {
                i++;
            }
            continue;
        }

        if (!in_block && source[i] == '/' && source[i + 1] == '*') {
            in_block = 1;
            i += 2;
            continue;
        }

        if (in_block && source[i] == '*' && source[i + 1] == '/') {
            in_block = 0;
            i += 2;
            continue;
        }

        i++;
    }

    return in_block;
}

void comments_reset_state(void)
{
}

int lexer_skip_comments(Lexer *lexer)
{
    if (lexer == NULL || lexer->source == NULL || lexer->index >= lexer->length) {
        return 0;
    }

    if (lexer->in_block_comment) {
        if (lexer->source[lexer->index] == '\n') {
            lexer->in_block_comment_code = 0;
            return 0;
        }

        if (lexer->source[lexer->index] == '*' &&
            lexer->index + 1 < lexer->length &&
            lexer->source[lexer->index + 1] == '/') {
            lexer->in_block_comment = 0;
            lexer->in_block_comment_code = 0;
            lexer->index += 2;
            return 1;
        }

        if (lexer->in_block_comment_code) {
            return 0;
        }

        if (lexer->source[lexer->index] == '*') {
            lexer->in_block_comment_code = 1;
            lexer->index++;
            while (lexer->index < lexer->length &&
                   (lexer->source[lexer->index] == ' ' || lexer->source[lexer->index] == '\t')) {
                lexer->index++;
            }
            return 1;
        }

        size_t start_index = lexer->index;
        while (lexer->index < lexer->length) {
            if (lexer->source[lexer->index] == '\n') {
                break;
            }
            if (lexer->source[lexer->index] == '*' &&
                lexer->index + 1 < lexer->length &&
                lexer->source[lexer->index + 1] == '/') {
                break;
            }
            if (lexer->source[lexer->index] == '*') {
                break;
            }
            lexer->index++;
        }
        return lexer->index > start_index ? 1 : 0;
    }

    if (lexer->source[lexer->index] == '/' && lexer->index + 1 < lexer->length) {
        char next = lexer->source[lexer->index + 1];

        if (next == '/') {
            lexer->index += 2;
            while (lexer->index < lexer->length && lexer->source[lexer->index] != '\n') {
                lexer->index++;
            }
            return 1;
        }

        if (next == '*') {
            lexer->in_block_comment = 1;
            lexer->in_block_comment_code = 0;
            lexer->index += 2;

            if (lexer->source[lexer->index] == '*' &&
                !(lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '/')) {
                lexer->in_block_comment_code = 1;
                lexer->index++;
                while (lexer->index < lexer->length &&
                       (lexer->source[lexer->index] == ' ' || lexer->source[lexer->index] == '\t')) {
                    lexer->index++;
                }
            }
            return 1;
        }
    }

    return 0;
}
