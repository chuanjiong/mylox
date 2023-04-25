
#include "value_array.h"
#include "memory.h"

void init_value_array(ValueArray *array)
{
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void write_value_array(ValueArray *array, Value value)
{
    if (array->capacity < array->count+1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void free_value_array(ValueArray *array)
{
    FREE_ARRAY(Value, array->values, array->capacity);
    init_value_array(array);
}

