#include "./lexer.h"
#include "utils.h"
#include <sys/types.h>

typedef struct {
    char *str;
    Position position;
} Lexer;

typedef struct {
    Token *data;
    uint length;
    uint capacity;
} TokenBuf;

typedef struct {
    Token token;
    char *error_message;
} ParseResult;

char *next_char(Lexer *l) {
    if (*l->str == '\n') {
        l->position.line += 1;
        l->position.col = 0;
    }
    l->position.col++;
    return l->str++;
}

void skip_whitespace(Lexer *l) {
    while (is_whitespace(*l->str)) {
        next_char(l);
    }
}

ParseResult parse_ident(Lexer *l) {
    char *start = l->str;
}

ParseResult parse_string(Lexer *l) {
    char *start = l->str;
}

Token *lex(char *input) {
    Lexer l = (Lexer) {
        .position = (Position) {
            .line = 1,
            .col = 1,
        },
        .str = input,
    };
    const int INITIAL_CAPACITY = sizeof(Token) * 4;
    TokenBuf b = (TokenBuf) {
        .data = malloc(INITIAL_CAPACITY),
        .length = 0,
        .capacity = INITIAL_CAPACITY,
    };

    for (;;) {
        switch (*l.str) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '_':
            parse_ident(&l);
            break;
        case '"':
            parse_string(&l);
            break;
        case '0' ... '9':
            break;
            
        }
    }
}
