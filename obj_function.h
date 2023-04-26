
#ifndef _OBJ_FUNCTION_H_
#define _OBJ_FUNCTION_H_

#include "object.h"
#include "obj_string.h"
#include "chunk.h"

typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString *name;
}ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

ObjFunction *new_function();

void free_function(ObjFunction *function);

ObjNative* newNative(NativeFn function);

#endif

