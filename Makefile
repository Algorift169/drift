CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -Iinclude
BUILD_DIR = build
TARGET = $(BUILD_DIR)/drift

SRCS = main.c token/token.c lexer/core/lexer.c lexer/keywords/logical_keywords.c lexer/keywords/identity_keywords.c lexer/comments/comments.c lexer/comments/executable_comments.c parser/core/parser.c parser/control_flow/if_parser.c parser/control_flow/repeat_parser.c parser/control_flow/for_parser.c parser/control_flow/while_parser.c parser/arrays/array_parser.c parser/arrays/select_parser.c interpreter/core/value.c interpreter/core/environment.c interpreter/core/input.c interpreter/core/intptr.c interpreter/loop/repeat.c interpreter/loop/for.c interpreter/loop/while.c interpreter/loop/break.c interpreter/loop/continue.c interpreter/arrays/array_value.c interpreter/arrays/array.c interpreter/operators/operator.c interpreter/operators/operator_identity.c interpreter/operators/operator_ternary.c
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

test: $(TARGET)
	@set -e; for test_file in tests/loop/break_test.df tests/loop/continue_test.df; do \
		output_file="$${test_file%.df}.out"; \
		$(TARGET) "$$test_file" > "$$output_file.actual"; \
		diff -u "$$output_file" "$$output_file.actual"; \
		rm -f "$$output_file.actual"; \
	done

.PHONY: all clean test
