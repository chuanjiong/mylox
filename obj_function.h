
#ifndef _OBJ_FUNCTION_H_
#define _OBJ_FUNCTION_H_

#include "object.h"
#include "obj_string.h"
#include "chunk.h"
#include "table.h"

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString *name;
}ObjFunction;

typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;

typedef struct {
  Obj obj;
  ObjString* name;
  Table methods;
} ObjClass;

typedef struct {
  Obj obj;
  ObjClass* klass;
  Table fields;
} ObjInstance;

typedef struct {
  Obj obj;
  Value receiver;
  ObjClosure* method;
} ObjBoundMethod;


typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

ObjFunction *new_function();

void free_function(ObjFunction *function);

ObjNative* newNative(NativeFn function);

ObjClosure* newClosure(ObjFunction* function);

ObjUpvalue* newUpvalue(Value* slot);

ObjClass* newClass(ObjString* name);

ObjInstance* newInstance(ObjClass* klass);

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);

#endif

