
#include "object.h"
#include "memory.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, obj_type)    (type*)allocate_object(sizeof(type), obj_type)

static Obj *allocate_object(size_t size, ObjType type)
{
    Obj *obj = (Obj *)reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}

static ObjString *allocate_string(char *chars, int length)
{
    ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString *copy_string(const char *src, int length)
{
    char *dst = ALLOCATE(char, length+1);
    memcpy(dst, src, length);
    dst[length] = 0;
    return allocate_string(dst, length);
}

ObjString* takeString(char* chars, int length) {
  return allocate_string(chars, length);
}

