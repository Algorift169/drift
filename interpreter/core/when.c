/* When execution compares one subject with ordered case values.  

It evaluates the body of the first case whose value matches the subject. If no case matches, the
    optional else body is executed. The when statement is useful for cases where you want to
    evaluate a single expression and perform different actions based on its value, providing a clear
    and concise way to express multiple conditional branches in your code. It is often used as an
    alternative to if-else statements when you have multiple discrete values to compare against, 
    making the code more readable and expressive.
*/

#include <stdio.h>

#include "drift/control_flow.h"
#include "drift/interpreter.h"
#include "drift/operator.h"
#include "drift/when.h"

static int execute_body(Statement *body, size_t count, Environment *environment)
{
    for (size_t i = 0; i < count; ++i) {
        int result = interpreter_execute(body[i], environment);
        if (result != DRIFT_EXECUTION_OK) {
            return result;
        }
    }
    return DRIFT_EXECUTION_OK;
}

// The interpreter_execute_when function evaluates a when statement by first evaluating the subject
// expression and then comparing it against each case value in order. If a match is found, the
// corresponding case body is executed. If no match is found, the optional else body is executed.
int interpreter_execute_when(WhenStatement *statement, Environment *environment)
{
    int ok = 0;
    int matched = 0;
    Value subject;

    if (statement == NULL || environment == NULL || statement->subject_text == NULL) {
        fprintf(stderr, "Runtime Error: Invalid when statement.\n");
        return DRIFT_EXECUTION_ERROR;
    }
    subject = interpreter_evaluate_expression(environment, statement->subject_text, &ok);
    if (!ok) {
        value_free(&subject);
        fprintf(stderr, "Runtime Error: Failed to evaluate when subject '%s'.\n", statement->subject_text);
        return DRIFT_EXECUTION_ERROR;
    }

    // we go through each case in order and evaluate its value. If the value matches the subject, 
    // we execute the corresponding body and return. If no case matches, we execute the else body.
    for (size_t i = 0; i < statement->case_count; ++i) {
        Value case_value = interpreter_evaluate_expression(environment, statement->cases[i].value_text, &ok);
        if (!ok) {
            value_free(&case_value);
            value_free(&subject);
            fprintf(stderr, "Runtime Error: Failed to evaluate when case '%s'.\n", statement->cases[i].value_text);
            return DRIFT_EXECUTION_ERROR;
        }
        // We use the operator_apply function to compare the subject and case value for equality. If
        // they are equal, we execute the corresponding body and return. If not, we continue to the
        // next case.
        Value comparison = operator_apply(OPERATOR_EQUAL_EQUAL, &subject, &case_value);
        matched = comparison.type == VALUE_BOOLEAN && comparison.boolean_value;
        value_free(&comparison);
        value_free(&case_value);
        if (matched) {
            int result = execute_body(statement->cases[i].body, statement->cases[i].body_count, environment);
            value_free(&subject);
            return result;
        }
    }
    value_free(&subject);
    return execute_body(statement->else_body, statement->else_count, environment);
}
