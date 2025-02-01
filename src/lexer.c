#include "./lexer.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct {
    Token *data;
    uint length;
    uint capacity;
} TokenBuf;

char *next_char(Lexer *l) {
    if (*l->str == '\n') {
        l->position.line += 1;
        l->position.col = 0;
    }
    l->position.col++;
    return ++l->str;
}

static inline char peek_char(Lexer *l) {
    return l->str[1];
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

LexResult parse_ident(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    while (is_alpha(peek_char(l)) || is_digit(peek_char(l))) {
        next_char(l);
    }

    Position end_position = l->position;

    uint size = (uint)(l->str - start) + 1;
    char *ident = calloc(1, size + 1);
    assert_ptr(ident);
    strncpy(ident, start, size);

    next_char(l);

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_IDENT, 
            .inner.ident = ident,
            .range = (Range) {
                .start = start_position,
                .end = end_position,
            }
        },
    };
}

LexResult parse_string(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    bool backslashed = false;

    for (;;) {
        char c = *next_char(l);
        if (c == '"' && !backslashed) {
            break;
        }
        backslashed = false;
        if (c == '\\') {
            backslashed = true;
        }
    }

    Position end_position = l->position;

    uint size = (uint)(l->str - start) + 1;

    char *string = calloc(1, size + 1);
    assert_ptr(string);
    strncpy(string, start, size);

    // Skip past the "
    next_char(l);

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_STRING, 
            .inner.string = string,
            .range = (Range) {
                .start = start_position,
                .end = end_position,
            }
        },
    };
}

LexResult parse_number(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;
    bool has_decimal = false;

    for (;;) {
        char c = peek_char(l);

        if (c == '.') {
            if (has_decimal) {
                return (LexResult) {.error_message = "Invalid suffix on decimal"};
            } else {
                has_decimal = true;
            }
        } else if (!is_digit(c)) {
            break;
        }

        next_char(l);
    }

    Position end_position = l->position;

    uint size = (uint)(l->str - start) + 1;
    char *number = calloc(1, size + 1);
    assert_ptr(number);
    strncpy(number, start, size);

    double res = atof(number);
    free(number);

    // Go to character after the number
    next_char(l);

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_NUMBER, 
            .inner.number = res,
            .range = (Range) {
                .start = start_position,
                .end = end_position,
            }
        },
    };
}

LexResult lex_next_tok(Lexer *l) {
    skip_whitespace(l);
    switch (*l->str) {
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        return parse_ident(l);
    case '"':
        return parse_string(l);
    case '0' ... '9':
        return parse_number(l);
    case '\0':
        return (LexResult) {.finished = true};
    default:
        return (LexResult) {.error_message = "Illegal character"};
    }
}

Lexer lex_init(char *input) {
    return (Lexer) {
        .position = (Position) {
            .line = 1,
            .col = 1,
        },
        .str = input,
    };
}
