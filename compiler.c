
#include "compiler.h"
#include "scanner.h"
#include "debug.h"
#include "obj_function.h"
#include "memory.h"

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

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
}Local;

typedef enum {
    TYPE_SCRIPT,
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_INITIALIZER,
}FunctionType;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef struct Compiler {
    struct Compiler *enclosing;
    ObjFunction *function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int local_count;
    Upvalue upvalues[UINT8_COUNT];
    int scope_depth;
}Compiler;

static Parser parser;
static Compiler *current = NULL;
static Chunk *compiling_chunk;

static Scanner scanner;

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
  bool hasSuperclass;
} ClassCompiler;
ClassCompiler* currentClass = NULL;

static Chunk *current_chunk()
{
    return &current->function->chunk;
}

static void init_parser()
{
    parser.scan_error = false;
    parser.parse_error = false;
    parser.need_sync = false;
}

static void init_compiler(Compiler *compiler, FunctionType type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function();
    current = compiler;
    if (type != TYPE_SCRIPT) {
    current->function->name = copy_string(parser.previous.start,
                                         parser.previous.length);
  }
    Local *local = &current->locals[current->local_count++];
    local->depth = 0;
    if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
    local->isCaptured = false;
}
static void emit_return();
static ObjFunction *end()
{
    emit_return();

    ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.scan_error && !parser.parse_error)
        disassemble_chunk(current_chunk(), function->name != NULL ? function->name->chars : "<script>");
#endif
    current = current->enclosing;
    return function;
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
        parser.current = scan_token(&scanner);
        if (parser.current.type != TOKEN_ERROR_UNEXPECTED_CHARACTER
            && parser.current.type != TOKEN_ERROR_UNTERMINATED_STRING) return;
        parser.scan_error = true;
        if (parser.current.type == TOKEN_ERROR_UNEXPECTED_CHARACTER)
            printf("[Line %d] scan error at '%.*s', unexpected character.\n", parser.current.line, parser.current.length, parser.current.start);
        else if (parser.current.type == TOKEN_ERROR_UNTERMINATED_STRING)
            printf("[Line %d] scan error at '%.*s', unterminated string.\n", parser.current.line, parser.current.length, parser.current.start);
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

static int emit_jump(uint8_t byte)
{
    emit_byte(byte);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

static void patch_jump(int offset)
{
    int jump = current_chunk()->count - offset - 2;
    if (jump > UINT16_MAX) parse_error(parser.previous, "Too much code to jump over.");
    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset+1] = jump & 0xff;
}

static void emit_loop(int loop)
{
    emit_byte(OP_LOOP);
    int offset = current_chunk()->count - loop + 2;
    if (offset > UINT16_MAX) parse_error(parser.previous, "Loop body too large.");
    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static void emit_return()
{
    if (current->type == TYPE_INITIALIZER) {
    emit_byte2(OP_GET_LOCAL, 0);
  } else {
    emit_byte(OP_NIL);
  }
    emit_byte(OP_RETURN);
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
static void statement();
static void var_declaration();
static void declaration();

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

static void begin_scope()
{
    current->scope_depth++;
}

static void end_scope()
{
    current->scope_depth--;
    while (current->local_count > 0 && current->locals[current->local_count-1].depth > current->scope_depth) {
        if (current->locals[current->local_count - 1].isCaptured) {
      emit_byte(OP_CLOSE_UPVALUE);
    } else {
      emit_byte(OP_POP);
    }
        current->local_count--;
    }
}

static void add_local(Token name)
{
    if (current->local_count == UINT8_COUNT) {
        parse_error(name, "Too many local variables in function.");
        return;
    }
    Local *local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static bool identifiers_equal(Token a, Token b)
{
    if (a.length != b.length) return false;
    return memcmp(a.start, b.start, a.length) == 0;
}

static void declare_variable()
{
    if (current->scope_depth == 0) return;
    for (int i=current->local_count-1; i>=0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) break;
        if (identifiers_equal(parser.previous, local->name))
            parse_error(parser.previous, "Already a variable with this name in this scope.");
    }
    add_local(parser.previous);
}

static uint8_t parse_variable(const char *message)
{
    consume(TOKEN_IDENTIFIER, message);
    declare_variable();
    if (current->scope_depth > 0) return 0;
    return identifier_constant(parser.previous);
}

static void mark_initialized()
{
    if (current->scope_depth == 0) return;
    current->locals[current->local_count-1].depth = current->scope_depth;
}

static void define_variable(uint8_t index)
{
    if (current->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit_byte2(OP_DEFINE_GLOBAL, index);
}

static int resolve_local(Compiler *compiler, Token name)
{
    for (int i=compiler->local_count-1; i>=0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiers_equal(local->name, name)) {
            if (local->depth == -1) parse_error(name, "Can't read local variable in its own initializer.");
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index,
                      bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;
  for (int i = 0; i < upvalueCount; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }
  if (upvalueCount == UINT8_COUNT) {
    parse_error(parser.previous, "Too many closure variables in function.");
    return 0;
  }
  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolve_local(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }
  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

static void named_variable(Token name, bool can_assign)
{
    uint8_t get_op, set_op;
    int index = resolve_local(current, name);
    if (index != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else if ((index = resolveUpvalue(current, name)) != -1) {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    }
    else {
        index = identifier_constant(name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }
    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_byte2(set_op, index);
    }
    else {
        emit_byte2(get_op, index);
    }
}

static void variable(bool can_assign)
{
    named_variable(parser.previous, can_assign);
}

static void and(bool can_assign)
{
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    parse_precedence(PREC_AND);
    patch_jump(end_jump);
}

static void or(bool can_assign)
{
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);
    patch_jump(else_jump);
    emit_byte(OP_POP);
    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static uint8_t argument_list()
{
    uint8_t arg_count = 0;
    if (parser.current.type != TOKEN_RIGHT_PAREN) {
        do {
            expression();
            if (arg_count == 255) {
                parse_error(parser.previous, "Can't have more than 255 arguments.");
            }
            arg_count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}

static void call(bool can_assign)
{
    uint8_t arg_count = argument_list();
    emit_byte2(OP_CALL, arg_count);
}

static void dot(bool can_assign)
{
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifier_constant(parser.previous);

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_byte2(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argument_list();
    emit_byte2(OP_INVOKE, name);
    emit_byte(argCount);
    }
    else {
        emit_byte2(OP_GET_PROPERTY, name);
    }
}

static void this(bool can_assign)
{
    if (currentClass == NULL) {
    parse_error(parser.previous, "Can't use 'this' outside of a class.");
    return;
  }

    variable(false);
}

static Token syntheticToken(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void super(bool can_assign)
{
    if (currentClass == NULL) {
    parse_error(parser.previous, "Can't use 'super' outside of a class.");
  } else if (!currentClass->hasSuperclass) {
    parse_error(parser.previous, "Can't use 'super' in a class with no superclass.");
  }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifier_constant(parser.previous);
    named_variable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argument_list();
    named_variable(syntheticToken("super"), false);
    emit_byte2(OP_SUPER_INVOKE, name);
    emit_byte(argCount);
  } else {
    named_variable(syntheticToken("super"), false);
    emit_byte2(OP_GET_SUPER, name);
  }
}

static void grouping(bool can_assign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = {grouping,    call,       PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,        NULL,       PREC_NONE},
    [TOKEN_DOT]             = {NULL,        dot,        PREC_CALL},
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
    [TOKEN_AND]             = {NULL,        and,        PREC_AND},
    [TOKEN_CLASS]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ELSE]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FALSE]           = {literal,     NULL,       PREC_NONE},
    [TOKEN_FOR]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FUN]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IF]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_NIL]             = {literal,     NULL,       PREC_NONE},
    [TOKEN_OR]              = {NULL,        or,         PREC_OR},
    [TOKEN_PRINT]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RETURN]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SUPER]           = {super,        NULL,       PREC_NONE},
    [TOKEN_THIS]            = {this,        NULL,       PREC_NONE},
    [TOKEN_TRUE]            = {literal,     NULL,       PREC_NONE},
    [TOKEN_VAR]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_WHILE]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EOF]             = {NULL,        NULL,       PREC_NONE},
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

static void block()
{
    while (parser.current.type != TOKEN_RIGHT_BRACE && parser.current.type != TOKEN_EOF) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void block_statement()
{
    begin_scope();
    block();
    end_scope();
}

static void if_statement()
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    int else_jump = emit_jump(OP_JUMP);
    patch_jump(then_jump);
    emit_byte(OP_POP);
    if (match(TOKEN_ELSE)) statement();
    patch_jump(else_jump);
}

static void while_statement()
{
    int loop = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop);
    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void for_statement()
{
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {}
    else if (match(TOKEN_VAR)) var_declaration();
    else expression_statement();
    int loop = current_chunk()->count;
    int exit_jump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }
    if (!match(TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);
        int inc_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
        emit_loop(loop);
        loop = inc_start;
        patch_jump(body_jump);
    }
    statement();
    emit_loop(loop);
    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }
    end_scope();
}

static void return_statement()
{
    if (current->type == TYPE_SCRIPT) {
        parse_error(parser.previous, "Can't return from top-level code.");
    }
    if (match(TOKEN_SEMICOLON)) {
        emit_return();
    } else {
        if (current->type == TYPE_INITIALIZER) {
      parse_error(parser.previous, "Can't return a value from an initializer.");
    }
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

static void statement()
{
    if (match(TOKEN_PRINT)) print_statement();
    else if (match(TOKEN_LEFT_BRACE)) block_statement();
    else if (match(TOKEN_IF)) if_statement();
    else if (match(TOKEN_WHILE)) while_statement();
    else if (match(TOKEN_FOR)) for_statement();
    else if (match(TOKEN_RETURN)) return_statement();
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

static void function(FunctionType type)
{
    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (parser.current.type != TOKEN_RIGHT_PAREN) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                parse_error(parser.previous, "Can't have more than 255 parameters.");
            }
            uint8_t constant = parse_variable("Expect parameter name.");
            define_variable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = end();
    emit_byte2(OP_CLOSURE, make_constant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
    emit_byte(compiler.upvalues[i].isLocal ? 1 : 0);
    emit_byte(compiler.upvalues[i].index);
  }
}

static void fun_declaration()
{
    uint8_t global = parse_variable("Expect function name.");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void method() {
  consume(TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifier_constant(parser.previous);
  FunctionType type = TYPE_METHOD;
  if (parser.previous.length == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }
  function(type);
  emit_byte2(OP_METHOD, constant);
}



static void class_declaration()
{
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifier_constant(parser.previous);
    declare_variable();

    emit_byte2(OP_CLASS, nameConstant);
    define_variable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);

    if (identifiers_equal(className, parser.previous)) {
      parse_error(parser.previous, "A class can't inherit from itself.");
    }

    begin_scope();
    add_local(syntheticToken("super"));
    define_variable(0);


    named_variable(className, false);
    emit_byte(OP_INHERIT);
    classCompiler.hasSuperclass = true;
  }




    named_variable(className, false);

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    while (parser.current.type != TOKEN_RIGHT_BRACE && parser.current.type != TOKEN_EOF) method();

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emit_byte(OP_POP);

    if (classCompiler.hasSuperclass) {
    end_scope();
  }

    currentClass = currentClass->enclosing;
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
    else if (match(TOKEN_FUN)) fun_declaration();
    else if (match(TOKEN_CLASS)) class_declaration();
    else statement();
    if (parser.need_sync) synchronize();
}

ObjFunction *compile(const char *source)
{
    init_scanner(&scanner, source);
    init_parser();
    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);
    advance();
    while (!match(TOKEN_EOF)) declaration();
    ObjFunction *function = end();
    return (!parser.scan_error && !parser.parse_error) ? function : NULL;
}

void markCompilerRoots() {
  Compiler* compiler = current;
  while (compiler != NULL) {
    markObject((Obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}

