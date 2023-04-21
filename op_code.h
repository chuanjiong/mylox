
#ifndef _OP_CODE_H_
#define _OP_CODE_H_

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_FALSE,
    OP_TRUE,
    OP_NOT,
    OP_NEGATE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_RETURN,
}OpCode;

#endif

