
#include "object.h"
#include "memory.h"
#include "vm.h"

Obj *allocate_object(size_t size, ObjType type)
{
    Obj *obj = (Obj *)reallocate(NULL, 0, size);
    obj->type = type;
    obj->isMarked = false;
    obj->next = vm.objects;
    vm.objects = obj;
    #ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)obj, size, type);
#endif
    return obj;
}

