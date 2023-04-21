
#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "chunk.h"

void disassemble_chunk(Chunk *chunk, const char *name);

int disassembleInstruction(Chunk *chunk, int offset);

void print_value(Value value);

#endif

