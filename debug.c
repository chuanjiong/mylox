
#include <stdio.h>
#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk *chunk, const char *name)
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
    uint8_t constant = chunk->code[offset+1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
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

