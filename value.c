
#include "value.h"
#include "memory.h"
#include "object.h"

void init_value_array(ValueArray* array)
{
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void write_value_array(ValueArray* array, Value value)
{
    if (array->capacity < array->count+1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void free_value_array(ValueArray* array)
{
    FREE_ARRAY(Value, array->values, array->capacity);
    init_value_array(array);
}

bool is_values_equal(Value a, Value b)
{
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:
            if (OBJ_TYPE(a) == OBJ_STRING && OBJ_TYPE(b) == OBJ_STRING) {
                ObjString *astr = AS_STRING(a);
                ObjString *bstr = AS_STRING(b);
                return astr->length == bstr->length && memcmp(astr->chars, bstr->chars, astr->length) == 0;
            }
            break;
        default: break;
    }
    return false;
}

