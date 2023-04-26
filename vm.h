
#ifndef _VM_H_
#define _VM_H_

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "table.h"
#include "obj_function.h"

#define FRAMES_MAX          (64)
#define STACK_MAX           (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t *ip;
    Value *slots;
}CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value *top;
    Table globals;
    Table strings;
    Obj *objects;
    ObjUpvalue* openUpvalues;
}VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
}InterpretResult;

extern VM vm;

void init_vm();

void free_vm();

InterpretResult interpret(const char *source);

#endif

