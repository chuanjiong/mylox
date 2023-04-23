
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"

VM vm;

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

static Value peek(int pos)
{
    return vm.top[-1-pos];
}

void init_vm()
{
    vm.top = vm.stack;
    vm.objects = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
}

void free_vm()
{
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    free_objects();
}

static void runtime_error(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);
  vm.top = vm.stack;
}

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}

static InterpretResult run()
{
#define READ_BYTE()         (*vm.ip++)
#define READ_CONSTANT()     (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING()       AS_STRING(READ_CONSTANT())
#define BINARY_OP(type, op) do { \
                                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
                                    runtime_error("Operands must be numbers."); \
                                    return INTERPRET_RUNTIME_ERROR; \
                                } \
                                double b = AS_NUMBER(pop()); \
                                double a = AS_NUMBER(pop()); \
                                push(type(a op b)); \
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

            case OP_NIL:        push(NIL_VAL);                      break;
            case OP_FALSE:      push(BOOL_VAL(false));              break;
            case OP_TRUE:       push(BOOL_VAL(true));               break;
            case OP_NOT:        push(BOOL_VAL(isFalsey(pop())));    break;

            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;

            case OP_EQUAL:
                {
                    Value b = pop();
                    Value a = pop();
                    push(BOOL_VAL(is_values_equal(a, b)));
                }
                break;

            case OP_GREATER:    BINARY_OP(BOOL_VAL, >);     break;
            case OP_LESS:       BINARY_OP(BOOL_VAL, <);     break;

            case OP_ADD:
                {
                    if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                        concatenate();
                    }
                    else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                        double b = AS_NUMBER(pop());
                        double a = AS_NUMBER(pop());
                        push(NUMBER_VAL(a+b));
                    }
                    else {
                        runtime_error("Operands must be two numbers or two strings.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                break;
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -);   break;
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *);   break;
            case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /);   break;

            case OP_PRINT: {
                print_value(pop());
                printf("\n");
                break;
            }

            case OP_POP: pop(); break;

            case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }

      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }

      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }

            case OP_RETURN:
                {

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
#undef READ_STRING
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

