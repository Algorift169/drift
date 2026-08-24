/* Interpreter entry points consume parsed statements in source order and report runtime errors. */

#ifndef DRIFT_INTERPRETER_H
#define DRIFT_INTERPRETER_H

#include "drift/environment.h"
#include "drift/parser.h"
#include "drift/statement.h"

/* Evaluates one parsed statement against the supplied runtime environment. */
int interpreter_execute(Statement statement, Environment *environment);

/* Evaluates expression text using the interpreter's existing expression rules. */
Value interpreter_evaluate_expression(Environment *environment, const char *expression, int *ok);

#endif
