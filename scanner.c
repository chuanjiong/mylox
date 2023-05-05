
#include "scanner.h"

void init_scanner(Scanner *scanner, const char *source)
{
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

static bool is_end(Scanner *scanner)
{
    return *scanner->current == 0;
}

static char peek(Scanner *scanner)
{
    return *scanner->current;
}

static char peek_next(Scanner *scanner)
{
    if (is_end(scanner)) return 0;
    return scanner->current[1];
}

static char advance(Scanner *scanner)
{
    scanner->current++;
    return scanner->current[-1];
}

static bool match(Scanner *scanner, char expected)
{
    if (is_end(scanner)) return false;
    if (*scanner->current != expected) return false;
    scanner->current++;
    return true;
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_alpha_(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static void skip_space(Scanner *scanner)
{
    while (1) {
        char c = peek(scanner);
        switch (c) {
            case '\n':
                scanner->line++;
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '/':
                if (peek_next(scanner) == '/') {
                    while (peek(scanner) != '\n' && !is_end(scanner)) advance(scanner);
                }
                else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token make_token(Scanner *scanner, TokenType type)
{
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = scanner->current - scanner->start;
    token.line = scanner->line;
    return token;
}

static Token string(Scanner *scanner)
{
    while (peek(scanner) != '"' && !is_end(scanner)) {
        if (peek(scanner) == '\n') scanner->line++;
        advance(scanner);
    }
    if (is_end(scanner)) return make_token(scanner, TOKEN_ERROR_UNTERMINATED_STRING);
    advance(scanner);
    return make_token(scanner, TOKEN_STRING);
}

static Token number(Scanner *scanner)
{
    while (is_digit(peek(scanner))) advance(scanner);
    if (peek(scanner) == '.' && is_digit(peek_next(scanner))) {
        advance(scanner);
        while (is_digit(peek(scanner))) advance(scanner);
    }
    return make_token(scanner, TOKEN_NUMBER);
}

static TokenType check_keyword(Scanner *scanner, int start, int length, const char *rest, TokenType type)
{
    if (scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0)
        return type;
    return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(Scanner *scanner)
{
    switch (scanner->start[0]) {
        case 'a': return check_keyword(scanner, 1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(scanner, 1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(scanner, 1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return check_keyword(scanner, 2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(scanner, 2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(scanner, 2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return check_keyword(scanner, 1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(scanner, 1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(scanner, 1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(scanner, 1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(scanner, 1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h': return check_keyword(scanner, 2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(scanner, 2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(scanner, 1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(scanner, 1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *scanner)
{
    while (is_alpha_(peek(scanner)) || is_digit(peek(scanner))) advance(scanner);
    return make_token(scanner, identifier_type(scanner));
}

Token scan_token(Scanner *scanner)
{
    skip_space(scanner);

    scanner->start = scanner->current;

    if (is_end(scanner)) return make_token(scanner, TOKEN_EOF);

    char c = advance(scanner);

    if (is_digit(c)) return number(scanner);
    if (is_alpha_(c)) return identifier(scanner);

    switch (c) {
        case '(': return make_token(scanner, TOKEN_LEFT_PAREN);
        case ')': return make_token(scanner, TOKEN_RIGHT_PAREN);
        case '{': return make_token(scanner, TOKEN_LEFT_BRACE);
        case '}': return make_token(scanner, TOKEN_RIGHT_BRACE);
        case '.': return make_token(scanner, TOKEN_DOT);
        case ',': return make_token(scanner, TOKEN_COMMA);
        case ';': return make_token(scanner, TOKEN_SEMICOLON);
        case '+': return make_token(scanner, TOKEN_PLUS);
        case '-': return make_token(scanner, TOKEN_MINUS);
        case '*': return make_token(scanner, TOKEN_STAR);
        case '/': return make_token(scanner, TOKEN_SLASH);
        case '!': return make_token(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return make_token(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '>': return make_token(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '<': return make_token(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '"': return string(scanner);
        default: break;
    }

    return make_token(scanner, TOKEN_ERROR_UNEXPECTED_CHARACTER);
}

