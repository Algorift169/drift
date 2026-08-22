#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/environment.h"

static int is_name_equal(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

Environment environment_create(void)
{
    Environment environment;
    int i;

    environment.count = 0;
    for (i = 0; i < DRIFT_ENVIRONMENT_CAPACITY; ++i) {
        environment.entries[i].name = NULL;
        environment.entries[i].value = value_create_string(NULL);
    }

    return environment;
}

void environment_free(Environment *environment)
{
    int i;

    if (environment == NULL) {
        return;
    }

    for (i = 0; i < environment->count; ++i) {
        free(environment->entries[i].name);
        value_free(&environment->entries[i].value);
    }

    environment->count = 0;
}

int environment_set(Environment *environment, const char *name, const Value *value)
{
    int i;
    char *copy_name;
    Value new_value;

    if (environment == NULL || name == NULL || value == NULL) {
        return 0;
    }

    for (i = 0; i < environment->count; ++i) {
        if (is_name_equal(environment->entries[i].name, name)) {
            value_free(&environment->entries[i].value);
            new_value = value_copy(value);
            environment->entries[i].value = new_value;
            return 1;
        }
    }

    if (environment->count >= DRIFT_ENVIRONMENT_CAPACITY) {
        return 0;
    }

    copy_name = (char *)malloc(strlen(name) + 1U);
    if (copy_name == NULL) {
        return 0;
    }

    strcpy(copy_name, name);
    new_value = value_copy(value);

    environment->entries[environment->count].name = copy_name;
    environment->entries[environment->count].value = new_value;
    environment->count++;
    return 1;
}

int environment_get(const Environment *environment, const char *name, Value *out_value)
{
    int i;

    if (environment == NULL || name == NULL || out_value == NULL) {
        return 0;
    }

    for (i = 0; i < environment->count; ++i) {
        if (is_name_equal(environment->entries[i].name, name)) {
            *out_value = value_copy(&environment->entries[i].value);
            return 1;
        }
    }

    return 0;
}

int environment_exists(const Environment *environment, const char *name)
{
    int i;

    if (environment == NULL || name == NULL) {
        return 0;
    }

    for (i = 0; i < environment->count; ++i) {
        if (is_name_equal(environment->entries[i].name, name)) {
            return 1;
        }
    }

    return 0;
}
