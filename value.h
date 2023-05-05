
#ifndef _VALUE_H_
#define _VALUE_H_

#include "obj_string.h"

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.

typedef uint64_t Value;

#define NUMBER_VAL(num) numToValue(num)
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)

#define OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline Value numToValue(double num) {
  Value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

#define AS_NUMBER(value)    valueToNum(value)
#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))


static inline double valueToNum(Value value) {
  double num;
  memcpy(&num, &value, sizeof(Value));
  return num;
}

#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)

#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#else

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



#endif

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

#define IS_CLASS(value)        is_obj_type(value, OBJ_CLASS)
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))

#define IS_INSTANCE(value)     is_obj_type(value, OBJ_INSTANCE)
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))

#define IS_BOUND_METHOD(value) is_obj_type(value, OBJ_BOUND_METHOD)
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))


static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

bool is_values_equal(Value a, Value b);

#endif

