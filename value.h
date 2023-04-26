
#ifndef _VALUE_H_
#define _VALUE_H_

#include "obj_string.h"

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ,
}ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj *obj;
    }as;
}Value;

#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = (value)}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = (value)}})
#define OBJ_VAL(value)      ((Value){VAL_OBJ, {.obj = (Obj *)(value)}})

#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJ(value)       ((value).as.obj)

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_STRING(value)    is_obj_type((value), OBJ_STRING)
#define IS_FUNCTION(value)  is_obj_type((value), OBJ_FUNCTION)

#define AS_STRING(value)    ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)  ((ObjFunction *)AS_OBJ(value))

#define IS_NATIVE(value)       is_obj_type(value, OBJ_NATIVE)

#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)

#define IS_CLOSURE(value)      is_obj_type(value, OBJ_CLOSURE)

#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))

static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

bool is_values_equal(Value a, Value b);

#endif

