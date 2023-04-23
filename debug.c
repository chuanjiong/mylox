
#include <stdio.h>
#include "debug.h"
#include "value.h"

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
        SIMPLE_INSTRUCTION(OP_RETURN);

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

