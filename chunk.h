
#ifndef _CHUNK_H_
#define _CHUNK_H_

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NEG,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_RETURN,
}OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    ValueArray constants;
}Chunk;

void initChunk(Chunk *chunk);

void writeChunk(Chunk *chunk, uint8_t byte, int line);

int addConstant(Chunk* chunk, Value value);

void freeChunk(Chunk *chunk);

#endif

