
#ifndef _OBJ_STRING_H_
#define _OBJ_STRING_H_

#include "object.h"

typedef struct {
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
}ObjString;

ObjString *copy_string(const char *src, int length);

ObjString *take_string(char *chars, int length);

void free_string(ObjString *string);

#endif

