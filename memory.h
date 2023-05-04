
#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "common.h"
#include "value.h"

#define GC_HEAP_GROW_FACTOR 2

#define ALLOCATE(type)                  (type*)reallocate(NULL, 0, sizeof(type))
#define FREE(type, ptr)                 reallocate(ptr, sizeof(type), 0)

#define ALLOCATE_ARRAY(type, size)      (type*)reallocate(NULL, 0, sizeof(type)*(size))
#define FREE_ARRAY(type, ptr, size)     reallocate(ptr, sizeof(type)*(size), 0)

#define GROW_CAPACITY(old)              ((old) < 8 ? 8 : (old)*2)

#define GROW_ARRAY(type, ptr, old_capacity, new_capacity) \
                                        (type*)reallocate(ptr, sizeof(type)*(old_capacity), sizeof(type)*(new_capacity))

void *reallocate(void *ptr, size_t old_size, size_t new_size);

void collectGarbage();
void markValue(Value value);
void markObject(Obj* object);

#endif

