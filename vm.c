
#include <stdio.h>
#include "vm.h"
#include "debug.h"

VM vm;

static void resetStack()
{
    vm.top = vm.stack;
}

void initVM()
{
    resetStack();
}

void freeVM()
{
}

static InterpretResult run()
{
    #define READ_BYTE()         (*vm.ip++)
    #define READ_CONSTANT()     (vm.chunk->constants.values[READ_BYTE()])
    #define BINARY_OP(op)       do { \
                                    double b = pop(); \
                                    double a = pop(); \
                                    push(a op b); \
                                } while (0)

    while (1) {
        #ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot=vm.stack; slot<vm.top; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, vm.ip-vm.chunk->code);
        #endif

        uint8_t instruction;
        switch (instruction=READ_BYTE()) {
            case OP_CONSTANT:
                {
                    Value constant = READ_CONSTANT();
                    push(constant);
                }
                break;

            case OP_NEG:
                push(-pop());
                break;

            case OP_ADD:
                BINARY_OP(+);
                break;

            case OP_SUB:
                BINARY_OP(-);
                break;

            case OP_MUL:
                BINARY_OP(*);
                break;

            case OP_DIV:
                BINARY_OP(/);
                break;


            case OP_RETURN:
                {
                    printValue(pop());
                    printf("\n");
                    return INTERPRET_OK;
                }
                break;

            default:
                break;
        }
    }
    return INTERPRET_OK;

    #undef BINARY_OP
    #undef READ_CONSTANT
    #undef READ_BYTE
}

InterpretResult interpret(Chunk* chunk)
{
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}

void push(Value value)
{
    *vm.top = value;
    vm.top++;
}

Value pop()
{
    vm.top--;
    return *vm.top;
}

