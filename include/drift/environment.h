#ifndef DRIFT_ENVIRONMENT_H
#define DRIFT_ENVIRONMENT_H

#include "drift/value.h"

#define DRIFT_ENVIRONMENT_CAPACITY 128

typedef struct {
    char *name;
    Value value;
} EnvironmentEntry;

typedef struct {
    EnvironmentEntry entries[DRIFT_ENVIRONMENT_CAPACITY];
    int count;
} Environment;

Environment environment_create(void);
void environment_free(Environment *environment);
int environment_set(Environment *environment, const char *name, const Value *value);
int environment_get(const Environment *environment, const char *name, Value *out_value);
int environment_exists(const Environment *environment, const char *name);

#endif
