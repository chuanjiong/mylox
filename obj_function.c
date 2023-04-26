
#include "obj_function.h"
#include "memory.h"

ObjFunction *new_function()
{
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    init_chunk(&function->chunk);
    return function;
}

void free_function(ObjFunction *function)
{
    free_chunk(&function->chunk);
    FREE(ObjFunction, function);
}

ObjNative* newNative(NativeFn function) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}
