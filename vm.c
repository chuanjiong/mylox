
#include "vm.h"
#include "compiler.h"
#include "debug.h"

#define STACK_MAX           (256)

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    Value stack[STACK_MAX];
    Value *top;
}VM;

static VM vm;

static void push(Value value)
{
    *vm.top = value;
    vm.top++;
}

static Value pop()
{
    vm.top--;
    return *vm.top;
}

void init_vm()
{
    vm.top = vm.stack;
}

void free_vm()
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
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, vm.ip-vm.chunk->code);
        #endif

        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT:
                {
                    Value constant = READ_CONSTANT();
                    push(constant);
                }
                break;

            case OP_NEGATE:
                push(-pop());
                break;

            case OP_ADD:
                BINARY_OP(+);
                break;

            case OP_SUBTRACT:
                BINARY_OP(-);
                break;

            case OP_MULTIPLY:
                BINARY_OP(*);
                break;

            case OP_DIVIDE:
                BINARY_OP(/);
                break;


            case OP_RETURN:
                {
                    print_value(pop());
                    printf("\n");
                    return INTERPRET_OK;
                }
                break;

            default:
                printf("Unknown instruction %d\n", instruction);
                return INTERPRET_RUNTIME_ERROR;
        }
    }
    return INTERPRET_OK;

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult interpret(const char *source)
{
    Chunk chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;
    InterpretResult result = run();

    free_chunk(&chunk);
    return result;
}

