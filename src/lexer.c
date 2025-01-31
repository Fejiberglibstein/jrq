#include "./lexer.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
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

char *peek_char(Lexer *l) {
    return l->str + 1;
}

void skip_whitespace(Lexer *l) {
    while (is_whitespace(*l->str)) {
        next_char(l);
    }
}

bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

bool is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

ParseResult parse_ident(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    char c = *peek_char(l);
    while (is_alpha(c) || is_digit(c)) {
        next_char(l);
        c = *peek_char(l);
    }

    uint size = (uint)(l->str - start) + 1;

    char *ident = malloc(size + 1);
    assert_ptr(ident);
    strncpy(ident, start, size);
    ident[size] = '\0';

    return (ParseResult) {
        .token = (Token) {
            .type = TOKEN_IDENT, 
            .inner.ident = ident,
            .range = (Range) {
                .start = start_position,
                .end = l->position,
            }
        },
    };
}

ParseResult parse_string(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    bool backslashed = false;

    char c = *peek_char(l);
    while (c != '"' && !backslashed) {
        backslashed = false;
        if (c == '\\') {
            backslashed = true;
        }

        next_char(l);
        c = *peek_char(l);
    }

    uint size = (uint)(l->str - start) + 1;

    char *ident = malloc(size + 1);
    assert_ptr(ident);
    strncpy(ident, start, size);
    ident[size] = '\0';

    return (ParseResult) {
        .token = (Token) {
            .type = TOKEN_IDENT, 
            .inner.ident = ident,
            .range = (Range) {
                .start = start_position,
                .end = l->position,
            }
        },
    };
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
