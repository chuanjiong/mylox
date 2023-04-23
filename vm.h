
#ifndef _VM_H_
#define _VM_H_

#include "common.h"
#include "chunk.h"
#include "object.h"
#include "table.h"

#define STACK_MAX           (256)

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    Value stack[STACK_MAX];
    Value *top;
    Obj *objects;
    Table strings;
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

