
#include "compiler.h"
#include "scanner.h"
#include "debug.h"

typedef struct {
    Token current;
    Token previous;
    bool scan_error;
    bool parse_error;
    bool need_sync;
}Parser;

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
    PREC_PRIMARY,
}Precedence;

typedef void (*ParseFunc)(bool can_assign);
typedef struct {
    ParseFunc prefix;
    ParseFunc infix;
    Precedence precedence;
}ParseRule;

static Parser parser;
static Chunk *compiling_chunk;

static Chunk *current_chunk()
{
    return compiling_chunk;
}

static void init_parser()
{
    parser.scan_error = false;
    parser.parse_error = false;
    parser.need_sync = false;
}

static void parse_error(Token token, const char *message)
{
    parser.parse_error = true;
    parser.need_sync = true;
    printf("[Line %d] parse error at '%.*s', %s\n", token.line, token.length, token.start, message);
}

static void advance()
{
    parser.previous = parser.current;
    while (1) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) return;
        parser.scan_error = true;
        printf("[Line %d] scan error at '%c', %s\n", parser.current.line, parser.current.unexpect, parser.current.start);
    }
}

static bool match(TokenType type)
{
    if (parser.current.type != type) return false;
    advance();
    return true;
}

static void consume(TokenType type, const char *message)
{
    if (parser.current.type != type) {
        parse_error(parser.current, message);
        return;
    }
    advance();
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
        parse_error(parser.previous, "Too many constants in one chunk.");
        return 0;
    }
    return index;
}

static void emit_constant(Value value)
{
    emit_byte2(OP_CONSTANT, make_constant(value));
}

static ParseRule *get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static void expression();

static void unary(bool can_assign)
{
    TokenType type = parser.previous.type;
    parse_precedence(PREC_UNARY);
    switch (type) {
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: break;
    }
}

static void binary(bool can_assign)
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

static void literal(bool can_assign)
{
    switch (parser.previous.type) {
        case TOKEN_NIL: emit_byte(OP_NIL); break;
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_TRUE: emit_byte(OP_TRUE); break;
        default: break;
    }
}

static void number(bool can_assign)
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(bool can_assign)
{
    emit_constant(OBJ_VAL(copy_string(parser.previous.start+1, parser.previous.length-2)));
}

static uint8_t identifier_constant(Token name)
{
    return make_constant(OBJ_VAL(copy_string(name.start, name.length)));
}

static uint8_t parse_variable(const char *message)
{
    consume(TOKEN_IDENTIFIER, message);
    return identifier_constant(parser.previous);
}

static void define_variable(uint8_t index)
{
    emit_byte2(OP_DEFINE_GLOBAL, index);
}

static void named_variable(Token name, bool can_assign)
{
    uint8_t index = identifier_constant(name);
    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_byte2(OP_SET_GLOBAL, index);
    }
    else {
        emit_byte2(OP_GET_GLOBAL, index);
    }
}

static void variable(bool can_assign)
{
    named_variable(parser.previous, can_assign);
}

static void grouping(bool can_assign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

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
        parse_error(parser.previous, "Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFunc infix = get_rule(parser.previous.type)->infix;
        infix(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) parse_error(parser.previous, "Invalid assignment target.");
}

static void expression()
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void expression_statement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

static void print_statement()
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after print expression.");
    emit_byte(OP_PRINT);
}

static void statement()
{
    if (match(TOKEN_PRINT)) print_statement();
    else expression_statement();
}

static void var_declaration()
{
    uint8_t name_idx = parse_variable("Expect variable name.");
    if (match(TOKEN_EQUAL)) expression();
    else emit_byte(OP_NIL);
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    define_variable(name_idx);
}

static void synchronize()
{
    parser.need_sync = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON || parser.previous.type == TOKEN_RIGHT_BRACE) return;
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
        }
        advance();
    }
}

static void declaration()
{
    if (match(TOKEN_VAR)) var_declaration();
    else statement();
    if (parser.need_sync) synchronize();
}

static void end()
{
    emit_byte(OP_RETURN);
#ifdef DEBUG_PRINT_CODE
    if (!parser.scan_error && !parser.parse_error) disassemble_chunk(current_chunk(), "code");
#endif
}

bool compile(const char *source, Chunk *chunk)
{
    init_scanner(source);
    init_parser();
    compiling_chunk = chunk;
    advance();
    while (!match(TOKEN_EOF)) declaration();
    end();
    return !parser.scan_error && !parser.parse_error;
}

