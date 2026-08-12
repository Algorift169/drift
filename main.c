#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/comments.h"
#include "drift/lexer.h"
#include "drift/parser.h"
#include "drift/interpreter.h"

static char *read_file_to_string(const char *path)
{
    FILE *file = fopen(path, "rb");
    char *buffer;
    long length;
    size_t read_size;

    if (file == NULL) {
        fprintf(stderr, "Error: unable to open file '%s'\n", path);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "Error: unable to seek file '%s'\n", path);
        return NULL;
    }

    length = ftell(file);
    if (length < 0) {
        fclose(file);
        fprintf(stderr, "Error: unable to determine file size for '%s'\n", path);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "Error: unable to rewind file '%s'\n", path);
        return NULL;
    }

    buffer = (char *)malloc((size_t)length + 1U);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "Error: out of memory while reading '%s'\n", path);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)length, file);
    fclose(file);

    if (read_size != (size_t)length) {
        free(buffer);
        fprintf(stderr, "Error: unable to read file '%s'\n", path);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static char *trim_whitespace(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return text;
}

static void statement_free(Statement *statement)
{
    if (statement == NULL) {
        return;
    }

    if (statement->type == STATEMENT_PRINT) {
        print_statement_free(&statement->as.print_statement);
    } else if (statement->type == STATEMENT_VARIABLE_DECLARATION) {
        variable_declaration_free(&statement->as.variable_declaration);
    }
}

static int execute_source(const char *source, Environment *environment)
{
    Lexer lexer;
    Token *tokens;
    size_t token_count = 0;
    Parser parser;
    Statement statement;
    int result = 0;

    lexer = lexer_create(source);
    tokens = lexer_scan_all(&lexer, &token_count);
    if (tokens == NULL) {
        return 1;
    }

    parser = parser_create(tokens, token_count);
    while (parser.index < parser.count) {
        Token *tok = &parser.tokens[parser.index];
        if (tok->type == TOKEN_EOF) {
            break;
        }
        if (tok->type == TOKEN_NEWLINE) {
            parser.index++;
            continue;
        }

        statement = parser_parse(&parser);
        result = interpreter_execute(statement, environment);
        statement_free(&statement);
        if (result != 0) {
            break;
        }
    }

    token_free_array(tokens, token_count);

    return result;
}

static int execute_file(const char *path)
{
    char *source;
    Environment environment = environment_create();
    int result = 0;

    source = read_file_to_string(path);
    if (source == NULL) {
        environment_free(&environment);
        return 1;
    }

    result = execute_source(source, &environment);

    free(source);
    environment_free(&environment);
    return result;
}

static void run_repl(void)
{
    char line_buffer[4096];
    char *input_buffer = NULL;
    size_t input_length = 0;
    Environment environment = environment_create();

    printf(">>> ");
    fflush(stdout);

    while (fgets(line_buffer, sizeof(line_buffer), stdin) != NULL) {
        size_t line_len = strlen(line_buffer);
        char *new_buf = (char *)realloc(input_buffer, input_length + line_len + 1);
        if (new_buf == NULL) {
            fprintf(stderr, "Error: out of memory in REPL\n");
            free(input_buffer);
            environment_free(&environment);
            return;
        }
        input_buffer = new_buf;
        if (input_length == 0) {
            strcpy(input_buffer, line_buffer);
        } else {
            strcat(input_buffer, line_buffer);
        }
        input_length += line_len;

        char *trimmed = trim_whitespace(input_buffer);
        if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
            break;
        }

        if (is_block_comment_open(input_buffer)) {
            printf("... ");
            fflush(stdout);
            continue;
        }

        if (trimmed[0] != '\0') {
            execute_source(input_buffer, &environment);
        }

        free(input_buffer);
        input_buffer = NULL;
        input_length = 0;

        printf(">>> ");
        fflush(stdout);
    }

    free(input_buffer);
    environment_free(&environment);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        return execute_file(argv[1]);
    }

    run_repl();
    return 0;
}
