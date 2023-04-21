
#ifndef _VALUE_H_
#define _VALUE_H_

#include "common.h"

typedef double Value;

typedef struct {
    int count;
    int capacity;
    Value *values;
}ValueArray;

void init_value_array(ValueArray* array);

void write_value_array(ValueArray* array, Value value);

void free_value_array(ValueArray* array);

#endif

