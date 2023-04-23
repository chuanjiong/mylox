
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

typedef void (*ParseFunc)(bool canAssign);
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

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
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

static void grouping(bool canAssign);
static void literal(bool canAssign);
static void number(bool canAssign);
static void string(bool canAssign);
static void variable(bool canAssign);
static void unary(bool canAssign);
static void binary(bool canAssign);

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
    [TOKEN_BANG]            = {unary,       NULL,       PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL,        binary,     PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL,        binary,     PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_LESS]            = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_NUMBER]          = {number,      NULL,       PREC_NONE},
    [TOKEN_STRING]          = {string,      NULL,       PREC_NONE},
    [TOKEN_IDENTIFIER]      = {variable,    NULL,       PREC_NONE},
    [TOKEN_AND]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_CLASS]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ELSE]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FALSE]           = {literal,     NULL,       PREC_NONE},
    [TOKEN_FOR]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FUN]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IF]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_NIL]             = {literal,     NULL,       PREC_NONE},
    [TOKEN_OR]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_PRINT]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RETURN]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SUPER]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_THIS]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_TRUE]            = {literal,     NULL,       PREC_NONE},
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

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefix(canAssign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFunc infix = get_rule(parser.previous.type)->infix;
        infix(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static void expression()
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void grouping(bool canAssign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void literal(bool canAssign)
{
    switch (parser.previous.type) {
        case TOKEN_NIL: emit_byte(OP_NIL); break;
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_TRUE: emit_byte(OP_TRUE); break;
        default: break;
    }
}

static void number(bool canAssign)
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(bool canAssign)
{
    emit_constant(OBJ_VAL(copy_string(parser.previous.start+1, parser.previous.length-2)));
}

static uint8_t identifierConstant(Token* name) {
  return make_constant(OBJ_VAL(copy_string(name->start,
                                         name->length)));
}

static uint8_t parseVariable(const char* errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
  emit_byte2(OP_DEFINE_GLOBAL, global);
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t arg = identifierConstant(&name);
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emit_byte2(OP_SET_GLOBAL, arg);
  } else {
    emit_byte2(OP_GET_GLOBAL, arg);
  }
}


static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign)
{
    TokenType type = parser.previous.type;
    parse_precedence(PREC_UNARY);
    switch (type) {
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: break;
    }
}

static void binary(bool canAssign)
{
    TokenType type = parser.previous.type;
    ParseRule *rule = get_rule(type);
    parse_precedence(rule->precedence+1);
    switch (type) {
        case TOKEN_PLUS: emit_byte(OP_ADD); break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
        case TOKEN_BANG_EQUAL: emit_byte2(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER: emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_byte2(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emit_byte2(OP_GREATER, OP_NOT); break;
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

static void statement();
static void declaration();

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(OP_POP);
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(OP_PRINT);
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();

  } else {
    expressionStatement();
  }
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        ; // Do nothing.
    }

    advance();
  }
}



static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emit_byte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON,
          "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }
  if (parser.panicMode) synchronize();
}

bool compile(const char *source, Chunk *chunk)
{
    init_scanner(source);
    compiling_chunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;
    advance();
    while (!match(TOKEN_EOF)) {
    declaration();
  }

    end_compiler();
    return !parser.hadError;
}

