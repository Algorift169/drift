CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -Iinclude
BUILD_DIR = build
TARGET = $(BUILD_DIR)/drift

SRCS = main.c token/token.c lexer/core/lexer.c lexer/keywords/logical_keywords.c lexer/keywords/identity_keywords.c lexer/comments/comments.c lexer/comments/executable_comments.c parser/core/parser.c parser/control_flow/if_parser.c parser/control_flow/repeat_parser.c parser/arrays/array_parser.c parser/arrays/select_parser.c interpreter/core/value.c interpreter/core/environment.c interpreter/core/input.c interpreter/core/intptr.c interpreter/arrays/array_value.c interpreter/arrays/array.c interpreter/operators/operator.c interpreter/operators/operator_identity.c
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
