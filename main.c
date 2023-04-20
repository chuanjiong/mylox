
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

//gcc *.c -o test

int main(int argc, char **argv)
{
    initVM();

    Chunk chunk;
    initChunk(&chunk);
    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    constant = addConstant(&chunk, 3.4);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_ADD, 123);

    constant = addConstant(&chunk, 5.6);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);

    writeChunk(&chunk, OP_DIV, 123);

    writeChunk(&chunk, OP_NEG, 123);

    writeChunk(&chunk, OP_RETURN, 123);
    disassembleChunk(&chunk, "test chunk");


    interpret(&chunk);

    freeChunk(&chunk);

    freeVM();
    return 0;
}

