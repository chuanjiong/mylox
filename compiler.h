
#ifndef _COMPILER_H_
#define _COMPILER_H_

#include "chunk.h"
#include "obj_function.h"

ObjFunction *compile(const char *source);

void markCompilerRoots();

#endif

