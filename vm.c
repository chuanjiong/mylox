
#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "obj_function.h"

VM vm;

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
    vm.openUpvalues = NULL;
      vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;
      vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
    init_table(&vm.globals);
    init_table(&vm.strings);
    defineNative("clock", clockNative);



}

void freeObject(Obj* object) {
  #ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
#endif
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
      case OBJ_CLOSURE: {
        ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues,
                 closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;

      case OBJ_CLASS: {
      FREE(ObjClass, object);
      break;
    }

    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      free_table(&instance->fields);
      FREE(ObjInstance, object);
      break;
    }
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

     free(vm.grayStack);
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
    ObjFunction* function = frame->closure->function;
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
  vm.openUpvalues = NULL;
  vm.frameCount = 0;
}

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
     ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE_ARRAY(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
     pop();
  pop();

    push(OBJ_VAL(result));
}

static bool call(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
    runtime_error("Expected %d arguments but got %d.",
        closure->function->arity, argCount);
    return false;
  }
  if (vm.frameCount == FRAMES_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure  = closure ;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.top - argCount - 1;
  return true;
}



static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm.top[-argCount - 1] = OBJ_VAL(newInstance(klass));
        return true;
      }

      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.top - argCount);
        vm.top -= argCount + 1;
        push(result);
        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      default:
        break; // Non-callable object type.
    }
  }
  runtime_error("Can only call functions and classes.");
  return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }
  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }
  return createdUpvalue;
}

static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static InterpretResult run()
{
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])

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

        disassembleInstruction(&frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
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

      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }

      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
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
                        closeUpvalues(frame->slots);
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

                case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] =
                captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.top - 1);
        pop();
        break;

        case OP_CLASS:
        push(OBJ_VAL(newClass(READ_STRING())));
        break;

        case OP_GET_PROPERTY: {
          if (!IS_INSTANCE(peek(0))) {
          runtime_error("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(0));
        ObjString* name = READ_STRING();

        Value value;
        if (table_get(&instance->fields, name, &value)) {
          pop(); // Instance.
          push(value);
          break;
        }
        runtime_error("Undefined property '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtime_error("Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek(1));
        table_set(&instance->fields, READ_STRING(), peek(0));
        Value value = pop();
        pop();
        push(value);
        break;
      }

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

    ObjClosure* closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);

    return run();
}

