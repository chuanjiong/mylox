
#ifndef _SCANNER_H_
#define _SCANNER_H_

#include "common.h"

typedef enum {
    TOKEN_LEFT_PAREN,           // (
    TOKEN_RIGHT_PAREN,          // )
    TOKEN_LEFT_BRACE,           // {
    TOKEN_RIGHT_BRACE,          // }
    TOKEN_DOT,                  // .
    TOKEN_COMMA,                // ,
    TOKEN_SEMICOLON,            // ;
    TOKEN_PLUS,                 // +
    TOKEN_MINUS,                // -
    TOKEN_STAR,                 // *
    TOKEN_SLASH,                // /
    TOKEN_BANG,                 // !
    TOKEN_BANG_EQUAL,           // !=
    TOKEN_EQUAL,                // =
    TOKEN_EQUAL_EQUAL,          // ==
    TOKEN_GREATER,              // >
    TOKEN_GREATER_EQUAL,        // >=
    TOKEN_LESS,                 // <
    TOKEN_LESS_EQUAL,           // <=
    TOKEN_NUMBER,               // 数字
    TOKEN_STRING,               // 字符串
    TOKEN_IDENTIFIER,           // 标识符
    // 关键字
    TOKEN_AND,
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_NIL,
    TOKEN_OR,
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,
    // 结束标志
    TOKEN_EOF,
    // 错误标志
    TOKEN_ERROR_UNEXPECTED_CHARACTER,
    TOKEN_ERROR_UNTERMINATED_STRING,
}TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
}Token;

typedef struct {
    const char *start;
    const char *current;
    int line;
}Scanner;

void init_scanner(Scanner *scanner, const char *source);

Token scan_token(Scanner *scanner);

#endif

