
#ifndef _VALUE_ARRAY_H_
#define _VALUE_ARRAY_H_

#include "value.h"

typedef struct {
    int count;
    int capacity;
    Value *values;
}ValueArray;

void init_value_array(ValueArray *array);

void write_value_array(ValueArray *array, Value value);

void free_value_array(ValueArray *array);

#endif

