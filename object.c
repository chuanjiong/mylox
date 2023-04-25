
#include "object.h"
#include "memory.h"
#include "vm.h"

Obj *allocate_object(size_t size, ObjType type)
{
    Obj *obj = ALLOCATE(Obj, size);
    obj->type = type;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}

