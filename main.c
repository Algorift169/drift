#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    int result;

    lexer = lexer_create(source);
    tokens = lexer_scan_all(&lexer, &token_count);
    if (tokens == NULL) {
        return 1;
    }

    parser = parser_create(tokens, token_count);
    statement = parser_parse(&parser);
    result = interpreter_execute(statement, environment);

    statement_free(&statement);
    token_free_array(tokens, token_count);

    return result;
}

static int execute_file(const char *path)
{
    char *source;
    char *line_start;
    char *newline_pos;
    Environment environment = environment_create();
    int result = 0;

    source = read_file_to_string(path);
    if (source == NULL) {
        environment_free(&environment);
        return 1;
    }

    line_start = source;
    while (*line_start != '\0') {
        newline_pos = strchr(line_start, '\n');
        if (newline_pos != NULL) {
            *newline_pos = '\0';
        }

        if (trim_whitespace(line_start)[0] != '\0') {
            char *trimmed = trim_whitespace(line_start);
            result = execute_source(trimmed, &environment);
            if (result != 0) {
                free(source);
                environment_free(&environment);
                return result;
            }
        }

        if (newline_pos == NULL) {
            break;
        }

        line_start = newline_pos + 1;
    }

    free(source);
    environment_free(&environment);
    return result;
}

static void run_repl(void)
{
    char buffer[4096];
    Environment environment = environment_create();

    printf(">>> ");
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        char *trimmed;
        int result;

        trimmed = trim_whitespace(buffer);
        if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
            break;
        }

        if (trimmed[0] == '\0') {
            printf(">>> ");
            continue;
        }

        result = execute_source(trimmed, &environment);
        if (result != 0) {
            printf(">>> ");
            continue;
        }

        printf(">>> ");
    }

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
