
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

//gcc *.c -o test

static void repl()
{
    char line[1024];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(line);
    }
}

static char *readFile(const char *file)
{
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
        printf("Could not open file %s.\n", file);
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    size_t fileSize = ftell(fp);
    rewind(fp);
    char *buffer = (char *)malloc(fileSize + 1);
    if (buffer == NULL) {
        printf("Not enough memory to read %s.\n", file);
        fclose(fp);
        return NULL;
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, fp);
    if (bytesRead < fileSize) {
        printf("Could not read file %s.\n", file);
        free(buffer);
        fclose(fp);
        return NULL;
    }
    buffer[bytesRead] = '\0';
    fclose(fp);
    return buffer;
}

static void runFile(const char *file)
{
    char *source = readFile(file);
    InterpretResult result = interpret(source);
    free(source);
    if (result == INTERPRET_COMPILE_ERROR)
        printf("compile error!\n");
    if (result == INTERPRET_RUNTIME_ERROR)
        printf("runtime error!\n");
}

int main(int argc, char **argv)
{
    initVM();

    if (argc == 1) {
        repl();
    }
    else if (argc == 2) {
        runFile(argv[1]);
    }
    else {
        printf("Usage: %s [script]\n", argv[0]);
    }

    freeVM();
    return 0;
}

