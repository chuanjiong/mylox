
#include <stdio.h>
#include "debug.h"
#include "value.h"

void disassemble_chunk(Chunk *chunk, const char *name)
{
    printf("== %s ==\n", name);
    for (int offset=0; offset<chunk->count;)
        offset = disassembleInstruction(chunk, offset);
}

static int simpleInstruction(const char *name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t index = chunk->code[offset+1];
    printf("%-16s %4d '", name, index);
    print_value(chunk->constants.values[index]);
    printf("'\n");
    return offset + 2;
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
    int next = 0;
    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            next = constantInstruction("op_constant", chunk, offset);
            break;

        case OP_NIL:
            next = simpleInstruction("op_nil", offset);
            break;

        case OP_FALSE:
            next = simpleInstruction("op_false", offset);
            break;

        case OP_TRUE:
            next = simpleInstruction("op_true", offset);
            break;

        case OP_NOT:
            next = simpleInstruction("op_not", offset);
            break;

        case OP_NEGATE:
            next = simpleInstruction("op_negate", offset);
            break;

        case OP_EQUAL:
            next = simpleInstruction("op_equal", offset);
            break;

        case OP_GREATER:
            next = simpleInstruction("op_greater", offset);
            break;

        case OP_LESS:
            next = simpleInstruction("op_less", offset);
            break;

        case OP_ADD:
            next = simpleInstruction("op_add", offset);
            break;

        case OP_SUBTRACT:
            next = simpleInstruction("op_sub", offset);
            break;

        case OP_MULTIPLY:
            next = simpleInstruction("op_mul", offset);
            break;

        case OP_DIVIDE:
            next = simpleInstruction("op_div", offset);
            break;

        case OP_RETURN:
            next = simpleInstruction("op_return", offset);
            break;

        default:
            printf("Unknown opcode %d\n", instruction);
            next = offset + 1;
            break;
    }
    return next;
}

void print_value(Value value)
{
    switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL: printf("nil"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
  }
}

