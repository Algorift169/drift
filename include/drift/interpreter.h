#ifndef DRIFT_INTERPRETER_H
#define DRIFT_INTERPRETER_H

#include "drift/environment.h"
#include "drift/parser.h"
#include "drift/statement.h"

int interpreter_execute(Statement statement, Environment *environment);

#endif
