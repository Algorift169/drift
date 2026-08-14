CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -Iinclude
BUILD_DIR = build
TARGET = $(BUILD_DIR)/drift

SRCS = main.c token/token.c lexer/lexer.c lexer/comments.c lexer/executable_comments.c parser/parser.c parser/if_parser.c parser/array_parser.c parser/select_parser.c interpreter/value.c interpreter/environment.c interpreter/input.c interpreter/intptr.c interpreter/array_value.c interpreter/array.c interpreter/operator.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
