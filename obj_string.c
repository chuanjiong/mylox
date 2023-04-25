
#include "obj_string.h"
#include "memory.h"
#include "vm.h"

static uint32_t hash_string(const char *str, int length)
{
    uint32_t hash = 2166136261u;
    for (int i=0; i<length; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619;
    }
    return hash;
}

static ObjString *allocate_string(char *chars, int length, uint32_t hash)
{
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->chars = chars;
    string->length = length;
    string->hash = hash;
    table_set(&vm.strings, string, NIL_VAL);
    return string;
}

ObjString *copy_string(const char *src, int length)
{
    uint32_t hash = hash_string(src, length);
    ObjString *interned = table_find_string(&vm.strings, src, length, hash);
    if (interned != NULL) return interned;
    char *chars = ALLOCATE_ARRAY(char, length+1);
    memcpy(chars, src, length);
    chars[length] = 0;
    return allocate_string(chars, length, hash);
}

ObjString *take_string(char *chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    ObjString *interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length+1);
        return interned;
    }
    return allocate_string(chars, length, hash);
}

void free_string(ObjString *string)
{
    FREE_ARRAY(char, string->chars, string->length+1);
    FREE(ObjString, string);
}

