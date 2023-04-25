
#ifndef _CHUNK_H_
#define _CHUNK_H_

#include "op_code.h"
#include "value_array.h"

typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    ValueArray constants;
}Chunk;

void init_chunk(Chunk *chunk);

void write_chunk(Chunk *chunk, uint8_t byte, int line);

int add_constant(Chunk *chunk, Value value);

void free_chunk(Chunk *chunk);

#endif

