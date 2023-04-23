
#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "common.h"

#define ALLOCATE(type, size)            (type*)reallocate(NULL, 0, sizeof(type)*(size))

#define GROW_CAPACITY(old)              ((old)<8?8:(old)*2)

#define GROW_ARRAY(type, ptr, old_capacity, new_capacity) \
                                        (type*)reallocate(ptr, sizeof(type)*old_capacity, sizeof(type)*new_capacity)

#define FREE_ARRAY(type, ptr, size)     reallocate(ptr, sizeof(type)*size, 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

void *reallocate(void *ptr, size_t old_size, size_t new_size);

void free_objects();

#endif

