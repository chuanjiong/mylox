
#ifndef _VM_H_
#define _VM_H_

#include "chunk.h"

#define STACK_MAX           (256)

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    Value stack[STACK_MAX];
    Value *top;
}VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
}InterpretResult;

void initVM();

void freeVM();

InterpretResult interpret(Chunk* chunk);

void push(Value value);

Value pop();

#endif
