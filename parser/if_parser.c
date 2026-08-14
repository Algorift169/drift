#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/parser.h"

static Token *parser_peek(Parser *parser)
{
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

static Token *parser_advance(Parser *parser)
{
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index++];
}

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

static int is_statement_terminator(Token *token)
{
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

static void append_statement(Statement **items, size_t *count, size_t *capacity, Statement statement)
{
    Statement *new_items;

    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0U) ? 4U : (*capacity * 2U);
        new_items = (Statement *)realloc(*items, new_capacity * sizeof(Statement));
        if (new_items == NULL) {
            fprintf(stderr, "Error: out of memory while building if statement body.\n");
            return;
        }
        *items = new_items;
        *capacity = new_capacity;
    }

    (*items)[(*count)++] = statement;
}

static void free_statement_list(Statement *body, size_t count)
{
    if (body == NULL) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (body[i].type == STATEMENT_PRINT) {
            print_statement_free(&body[i].as.print_statement);
        } else if (body[i].type == STATEMENT_VARIABLE_DECLARATION) {
            variable_declaration_free(&body[i].as.variable_declaration);
        } else if (body[i].type == STATEMENT_INPUT) {
            if (body[i].as.input_statement.items != NULL) {
                for (size_t j = 0; j < body[i].as.input_statement.count; ++j) {
                    free(body[i].as.input_statement.items[j].prompt);
                    free(body[i].as.input_statement.items[j].target_name);
                }
                free(body[i].as.input_statement.items);
                body[i].as.input_statement.items = NULL;
                body[i].as.input_statement.count = 0;
            }
        } else if (body[i].type == STATEMENT_IF) {
            if_statement_free(&body[i].as.if_statement);
        }
    }

    free(body);
}

static char *read_expression_until_colon(Parser *parser)
{
    size_t length = 0;
    char *result = NULL;
    Token *token;

    while ((token = parser_peek(parser)) != NULL && token->type != TOKEN_COLON && token->type != TOKEN_EOF && !is_statement_terminator(token)) {
        size_t token_len = token->value != NULL ? strlen(token->value) : 0U;
        char *new_result = (char *)realloc(result, length + token_len + 2U);
        if (new_result == NULL) {
            free(result);
            return NULL;
        }
        result = new_result;
        if (length > 0U) {
            result[length++] = ' ';
        }
        if (token_len > 0U) {
            memcpy(result + length, token->value, token_len);
            length += token_len;
        }
        result[length] = '\0';
        parser_advance(parser);
    }

    if (result == NULL) {
        result = (char *)malloc(1U);
        if (result != NULL) {
            result[0] = '\0';
        }
    }

    return result;
}

static int parse_if_body(Parser *parser, Statement **out_body, size_t *out_count)
{
    Statement *body = NULL;
    size_t body_count = 0;
    size_t capacity = 0;
    Token *token;

    if (out_body == NULL || out_count == NULL) {
        return 0;
    }

    *out_body = NULL;
    *out_count = 0;

    while (1) {
        token = parser_peek(parser);
        if (token == NULL || token->type == TOKEN_EOF) {
            break;
        }

        if (token->type == TOKEN_NEWLINE) {
            parser_advance(parser);
            continue;
        }

        if (token->type == TOKEN_ELIF || token->type == TOKEN_ELSE) {
            break;
        }

        {
            Statement statement = parser_parse(parser);
            append_statement(&body, &body_count, &capacity, statement);
        }
    }

    *out_body = body;
    *out_count = body_count;
    return 1;
}


int parse_if_statement(Parser *parser, Statement *statement)
{
    IfStatement if_statement;
    IfBranch branch;
    Statement *body = NULL;
    size_t body_count = 0;
    Token *token;

    if (parser == NULL || statement == NULL) {
        return 0;
    }

    memset(&if_statement, 0, sizeof(if_statement));
    parser_advance(parser);

    branch.condition_text = NULL;
    branch.body = NULL;
    branch.body_count = 0;

    if (parser_peek(parser) == NULL) {
        fprintf(stderr, "Syntax Error: Expected condition after 'if'.\n");
        return 0;
    }

    branch.condition_text = read_expression_until_colon(parser);
    if (branch.condition_text == NULL) {
        fprintf(stderr, "Error: out of memory while parsing if condition.\n");
        return 0;
    }

    if (!parser_expect(parser, TOKEN_COLON, "Syntax Error: Expected ':' after if condition.")) {
        free(branch.condition_text);
        return 0;
    }

    if (!parse_if_body(parser, &body, &body_count)) {
        free(branch.condition_text);
        return 0;
    }
    branch.body = body;
    branch.body_count = body_count;

    if_statement.branches = (IfBranch *)calloc(1U, sizeof(IfBranch));
    if (if_statement.branches == NULL) {
        free(branch.condition_text);
        if (body != NULL) {
            free_statement_list(body, body_count);
        }
        return 0;
    }

    if_statement.branches[0] = branch;
    if_statement.branch_count = 1;

    while (1) {
        token = parser_peek(parser);
        if (token == NULL || token->type == TOKEN_EOF) {
            break;
        }

        if (token->type == TOKEN_ELIF) {
            IfBranch next_branch;
            Statement *next_body = NULL;
            size_t next_body_count = 0;
            IfBranch *new_branches;

            parser_advance(parser);
            next_branch.condition_text = read_expression_until_colon(parser);
            if (next_branch.condition_text == NULL) {
                fprintf(stderr, "Error: out of memory while parsing elif condition.\n");
                if_statement_free(&if_statement);
                return 0;
            }
            if (!parser_expect(parser, TOKEN_COLON, "Syntax Error: Expected ':' after elif condition.")) {
                free(next_branch.condition_text);
                if_statement_free(&if_statement);
                return 0;
            }
            if (!parse_if_body(parser, &next_body, &next_body_count)) {
                free(next_branch.condition_text);
                if_statement_free(&if_statement);
                return 0;
            }
            next_branch.body = next_body;
            next_branch.body_count = next_body_count;

            new_branches = (IfBranch *)realloc(if_statement.branches, (if_statement.branch_count + 1U) * sizeof(IfBranch));
            if (new_branches == NULL) {
                fprintf(stderr, "Error: out of memory while parsing if chain.\n");
                free(next_branch.condition_text);
                if (next_body != NULL) {
                    free_statement_list(next_body, next_body_count);
                }
                if_statement_free(&if_statement);
                return 0;
            }
            if_statement.branches = new_branches;
            if_statement.branches[if_statement.branch_count++] = next_branch;
            continue;
        }

        if (token->type == TOKEN_ELSE) {
            Statement *else_body = NULL;
            size_t else_count = 0;
            parser_advance(parser);
            if (!parser_expect(parser, TOKEN_COLON, "Syntax Error: Expected ':' after else.")) {
                if_statement_free(&if_statement);
                return 0;
            }
            if (!parse_if_body(parser, &else_body, &else_count)) {
                if_statement_free(&if_statement);
                return 0;
            }
            if_statement.else_body = else_body;
            if_statement.else_count = else_count;
            break;
        }

        break;
    }

    statement->type = STATEMENT_IF;
    statement->as.if_statement = if_statement;
    return 1;
}

void if_statement_free(IfStatement *statement)
{
    if (statement == NULL) {
        return;
    }

    if (statement->branches != NULL) {
        for (size_t i = 0; i < statement->branch_count; ++i) {
            free(statement->branches[i].condition_text);
            if (statement->branches[i].body != NULL) {
                free_statement_list(statement->branches[i].body, statement->branches[i].body_count);
            }
        }
        free(statement->branches);
        statement->branches = NULL;
        statement->branch_count = 0;
    }

    if (statement->else_body != NULL) {
        free_statement_list(statement->else_body, statement->else_count);
    }
    statement->else_body = NULL;
    statement->else_count = 0;
}
