/* Interpreter entry points consume parsed statements in source order and report runtime errors. */

#ifndef DRIFT_INTERPRETER_H
#define DRIFT_INTERPRETER_H

#include "drift/environment.h"
#include "drift/break.h"
#include "drift/continue.h"
#include "drift/control_flow.h"
#include "drift/parser.h"
#include "drift/statement.h"

/* Evaluates one parsed statement against the supplied runtime environment. */
int interpreter_execute(Statement statement, Environment *environment);

/* Evaluates expression text using the interpreter's existing expression rules. */
Value interpreter_evaluate_expression(Environment *environment, const char *expression, int *ok);

/* Executes the parsed basic for and while loop forms. */
int interpreter_execute_for(ForStatement *for_statement, Environment *environment);
int interpreter_execute_while(WhileStatement *while_statement, Environment *environment);

#endif
