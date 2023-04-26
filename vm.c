
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "obj_function.h"

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
static void defineNative(const char* name, NativeFn function) {
  push(OBJ_VAL(copy_string(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

void init_vm()
{
    vm.frameCount = 0;
    vm.top = vm.stack;
    vm.objects = NULL;
    init_table(&vm.globals);
    init_table(&vm.strings);
    defineNative("clock", clockNative);
}

static void freeObject(Obj* object) {
  switch (object->type) {
    case OBJ_STRING: {
      free_string((ObjString*)object);
      break;
    }
    case OBJ_FUNCTION: {
        free_function((ObjFunction*)object);
        break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
  }
}

void free_vm()
{
    free_table(&vm.globals);
    free_table(&vm.strings);
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}

static void runtime_error(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ",
            function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }
  vm.top = vm.stack;
  vm.frameCount = 0;
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
    char* chars = ALLOCATE_ARRAY(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    push(OBJ_VAL(result));
}

static bool call(ObjFunction* function, int argCount) {
    if (argCount != function->arity) {
    runtime_error("Expected %d arguments but got %d.",
        function->arity, argCount);
    return false;
  }
  if (vm.frameCount == FRAMES_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm.top - argCount - 1;
  return true;
}



static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_FUNCTION:
        return call(AS_FUNCTION(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.top - argCount);
        vm.top -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }
  runtime_error("Can only call functions and classes.");
  return false;
}

static InterpretResult run()
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->function->chunk.constants.values[READ_BYTE()])

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

        disassembleInstruction(&frame->function->chunk,
        (int)(frame->ip - frame->function->chunk.code));
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
        table_set(&vm.globals, name, peek(0));
        pop();
        break;
      }

      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        if (!table_get(&vm.globals, name, &value)) {
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }

      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (table_set(&vm.globals, name, peek(0))) {
          table_del(&vm.globals, name);
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }

      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }

      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }

      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }

      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }

      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }

            case OP_RETURN:
                {

                        Value result = pop();
                        vm.frameCount--;
                        if (vm.frameCount == 0) {
                        pop();
                        return INTERPRET_OK;
                        }

                        vm.top = frame->slots;
                        push(result);
                        frame = &vm.frames[vm.frameCount - 1];
                        break;
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
#undef READ_SHORT
#undef READ_BYTE
}

InterpretResult interpret(const char *source)
{
    ObjFunction *function = compile(source);

    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));

    call(function, 0);

    return run();
}

