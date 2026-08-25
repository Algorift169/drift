/* When runtime execution is kept separate from the general interpreter. The when statement 
is a conditional execution form that evaluates a subject expression and runs the body of the 
first case whose condition matches the subject. If no case matches, the optional else body is
executed.
*/

#ifndef DRIFT_WHEN_H
#define DRIFT_WHEN_H

#include "drift/ast.h"
#include "drift/environment.h"

/* Executes one parsed when statement. */
int interpreter_execute_when(WhenStatement *statement, Environment *environment);

#endif