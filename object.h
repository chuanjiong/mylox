
#ifndef _OBJECT_H_
#define _OBJECT_H_

#include "common.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
}ObjType;

typedef struct Obj {
    ObjType type;
    struct Obj *next;
}Obj;

#define ALLOCATE_OBJ(type, obj_type)        (type*)allocate_object(sizeof(type), obj_type)

Obj *allocate_object(size_t size, ObjType type);

#endif

