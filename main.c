
#include "vm.h"

//gcc *.c -o test

static void run_prompt()
{
    char line[1024] = {0};
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin))
            break;
        InterpretResult result = interpret(line);
        if (result == INTERPRET_COMPILE_ERROR)
            printf("compile error!\n");
        else if (result == INTERPRET_RUNTIME_ERROR)
            printf("runtime error!\n");
    }
}

static char *read_file(const char *file)
{
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
        printf("Could not open file %s\n", file);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    rewind(fp);
    char *buf = (char *)malloc(file_size+1);
    if (buf == NULL) {
        printf("Not enough memory to read %s\n", file);
        fclose(fp);
        return NULL;
    }
    int r = fread(buf, 1, file_size, fp);
    if (r != file_size) {
        printf("Could not read file %s\n", file);
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[file_size] = 0;
    fclose(fp);
    return buf;
}

static void run_file(const char *file)
{
    char *source = read_file(file);
    if (source == NULL)
        return;
    printf("======== run: %s ========\n", file);
    InterpretResult result = interpret(source);
    free(source);
    if (result == INTERPRET_COMPILE_ERROR)
        printf("compile error!\n");
    else if (result == INTERPRET_RUNTIME_ERROR)
        printf("runtime error!\n");
}

int main(int argc, char **argv)
{
    init_vm();
    if (argc == 1)
        run_prompt();
    else {
        for (int i=1; i<argc; i++)
            run_file(argv[i]);
    }
    free_vm();
    return 0;
}

