#ifndef DRIFT_OPERATOR_TERNARY_H
#define DRIFT_OPERATOR_TERNARY_H

#include "drift/value.h"

Value operator_apply_ternary(const Value *condition, const Value *true_value, const Value *false_value);

#endif