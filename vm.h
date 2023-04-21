
#ifndef _VM_H_
#define _VM_H_

#include "common.h"

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
}InterpretResult;

void init_vm();

void free_vm();

InterpretResult interpret(const char *source);

#endif

