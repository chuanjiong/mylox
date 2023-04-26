
#include <stdio.h>
#include "debug.h"
#include "value.h"
#include "obj_function.h"

void disassemble_chunk(Chunk *chunk, const char *name)
{
    printf("== %s ==\n", name);
    for (int offset=0; offset<chunk->count;)
        offset = disassembleInstruction(chunk, offset);
}

static int simple_instruction(const char *name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t index = chunk->code[offset+1];
    printf("%-16s %4d '", name, index);
    print_value(chunk->constants.values[index]);
    printf("'\n");
    return offset + 2;
}

static int byteInstruction(const char* name, Chunk* chunk,
                           int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jumpInstruction(const char* name, int sign,
                           Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset,
         offset + 3 + sign * jump);
  return offset + 3;
}

int disassembleInstruction(Chunk *chunk, int offset)
{
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset-1]) {
        printf("   | ");
    }
    else {
        printf("%4d ", chunk->lines[offset]);
    }
    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        #define SIMPLE_INSTRUCTION(op)  case op: return simple_instruction(#op, offset)

        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);

        SIMPLE_INSTRUCTION(OP_NIL);
        SIMPLE_INSTRUCTION(OP_FALSE);
        SIMPLE_INSTRUCTION(OP_TRUE);
        SIMPLE_INSTRUCTION(OP_NOT);
        SIMPLE_INSTRUCTION(OP_NEGATE);
        SIMPLE_INSTRUCTION(OP_EQUAL);
        SIMPLE_INSTRUCTION(OP_GREATER);
        SIMPLE_INSTRUCTION(OP_LESS);
        SIMPLE_INSTRUCTION(OP_ADD);
        SIMPLE_INSTRUCTION(OP_SUBTRACT);
        SIMPLE_INSTRUCTION(OP_MULTIPLY);
        SIMPLE_INSTRUCTION(OP_DIVIDE);
        SIMPLE_INSTRUCTION(OP_PRINT);
        SIMPLE_INSTRUCTION(OP_POP);
        SIMPLE_INSTRUCTION(OP_RETURN);

        case OP_DEFINE_GLOBAL:
      return constant_instruction("OP_DEFINE_GLOBAL", chunk,
                                 offset);

        case OP_GET_GLOBAL:
      return constant_instruction("OP_GET_GLOBAL", chunk, offset);

      case OP_SET_GLOBAL:
      return constant_instruction("OP_SET_GLOBAL", chunk, offset);

      case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);

      case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);

      case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);

      case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);

      case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
      case OP_CLOSURE: {
      offset++;
      uint8_t constant = chunk->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      print_value(chunk->constants.values[constant]);
      printf("\n");
      ObjFunction* function = AS_FUNCTION(
          chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                     %s %d\n",
               offset - 2, isLocal ? "local" : "upvalue", index);
      }
      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simple_instruction("OP_CLOSE_UPVALUE", offset);

        default:
            printf("Unknown opcode %d\n", instruction);
            return offset+1;

        #undef SIMPLE_INSTRUCTION
    }
}

void print_object(Value value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_FUNCTION:

        printf("<fn %s>", AS_FUNCTION(value)->name == NULL? "<script>":AS_FUNCTION(value)->name->chars); break;
        case OBJ_NATIVE:
      printf("<native fn>");
      break;
      case OBJ_CLOSURE:
    //   print_function(AS_CLOSURE(value)->function);
      break;
      case OBJ_UPVALUE:
      printf("upvalue");
      break;
    }
}

void print_value(Value value)
{
    switch (value.type) {
        case VAL_NIL: printf("nil"); break;
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: print_object(value); break;
    }
}

