
#include "compiler.h"
#include "scanner.h"
#include "debug.h"


typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // != ==
    PREC_COMPARISON,    // > >= < <=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! -
    PREC_CALL,          // . ()
    PREC_PRIMARY
}Precedence;

typedef void (*ParseFunc)();
typedef struct {
    ParseFunc prefix;
    ParseFunc infix;
    Precedence precedence;
}ParseRule;




typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
}Parser;

Parser parser;

static Chunk *compiling_chunk;

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}

static void error(const char* message) {
  errorAt(&parser.previous, message);
}

static void advance()
{
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static Chunk *current_chunk()
{
    return compiling_chunk;
}

static void emit_byte(uint8_t byte)
{
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_byte2(uint8_t byte1, uint8_t byte2)
{
    emit_byte(byte1);
    emit_byte(byte2);
}

static uint8_t make_constant(Value value)
{
    int index = add_constant(current_chunk(), value);
    if (index > 255) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return index;
}

static void emit_constant(Value value)
{
    emit_byte2(OP_CONSTANT, make_constant(value));
}

static void grouping();
static void number();
static void unary();
static void binary();

static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = {grouping,    NULL,       PREC_NONE},
    [TOKEN_RIGHT_PAREN]     = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,        NULL,       PREC_NONE},
    [TOKEN_DOT]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_COMMA]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SEMICOLON]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_PLUS]            = {NULL,        binary,     PREC_TERM},
    [TOKEN_MINUS]           = {unary,       binary,     PREC_TERM},
    [TOKEN_STAR]            = {NULL,        binary,     PREC_FACTOR},
    [TOKEN_SLASH]           = {NULL,        binary,     PREC_FACTOR},
    [TOKEN_BANG]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EQUAL]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL,        NULL,       PREC_NONE},
    [TOKEN_GREATER]         = {NULL,        NULL,       PREC_NONE},
    [TOKEN_GREATER_EQUAL]   = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LESS]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LESS_EQUAL]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_NUMBER]          = {number,      NULL,       PREC_NONE},
    [TOKEN_STRING]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IDENTIFIER]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_AND]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_CLASS]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ELSE]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FALSE]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FOR]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FUN]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IF]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_NIL]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_OR]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_PRINT]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RETURN]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SUPER]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_THIS]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_TRUE]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_VAR]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_WHILE]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EOF]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ERROR]           = {NULL,        NULL,       PREC_NONE},
};

static ParseRule *get_rule(TokenType type)
{
    return &rules[type];
}

static void parse_precedence(Precedence precedence)
{
    advance();
    ParseFunc prefix = get_rule(parser.previous.type)->prefix;
    if (prefix == NULL) {
        error("Expect expression.");
        return;
    }
    prefix();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFunc infix = get_rule(parser.previous.type)->infix;
        infix();
    }
}

static void expression()
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void grouping()
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number()
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(value);
}

static void unary()
{
    TokenType type = parser.previous.type;
    parse_precedence(PREC_UNARY);
    switch (type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: break;
    }
}

static void binary()
{
    TokenType type = parser.previous.type;
    ParseRule *rule = get_rule(type);
    parse_precedence(rule->precedence+1);
    switch (type) {
        case TOKEN_PLUS: emit_byte(OP_ADD); break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
        default: break;
    }
}

static void end_compiler()
{
    emit_byte(OP_RETURN);

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
}

bool compile(const char *source, Chunk *chunk)
{
    init_scanner(source);
    compiling_chunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;
    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    end_compiler();
    return !parser.hadError;
}

