
#ifndef _VALUE_H_
#define _VALUE_H_

#include "common.h"

typedef double Value;

typedef struct {
    int count;
    int capacity;
    Value *values;
}ValueArray;

void initValueArray(ValueArray* array);

void writeValueArray(ValueArray* array, Value value);

void freeValueArray(ValueArray* array);

void printValue(Value value);

#endif

