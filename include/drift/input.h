#ifndef DRIFT_INPUT_H
#define DRIFT_INPUT_H

#include <stddef.h>

#include "drift/value.h"

char *drift_read_line_from_stdin(const char *prompt);
int drift_parse_input_value(const char *text, Value *out_value);
int drift_prompt_and_store(const char *prompt, const char *target_name, Value *out_value);

#endif
