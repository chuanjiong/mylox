
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, obj_type)    (type*)allocate_object(sizeof(type), obj_type)

static Obj *allocate_object(size_t size, ObjType type)
{
    Obj *obj = (Obj *)reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}

static ObjString *allocate_string(char *chars, int length, uint32_t hash)
{
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NIL_VAL);
    return string;
}

static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString *copy_string(const char *src, int length)
{
    uint32_t hash = hashString(src, length);
    ObjString* interned = tableFindString(&vm.strings, src, length,
                                        hash);
  if (interned != NULL) return interned;
    char *dst = ALLOCATE(char, length+1);
    memcpy(dst, src, length);
    dst[length] = 0;
    return allocate_string(dst, length, hash);
}

ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
    return allocate_string(chars, length, hash);
}

