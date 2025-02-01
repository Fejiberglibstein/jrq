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

LexResult parse_ident(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    char c = *start;
    while (is_alpha(c) || is_digit(c)) {
        c = *next_char(l);
    }

    uint size = (uint)(l->str - start);

    char *ident = malloc(size + 1);
    assert_ptr(ident);
    strncpy(ident, start, size);
    ident[size] = '\0';

    return (LexResult) {
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

LexResult parse_string(Lexer *l) {
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

    uint size = (uint)(l->str - start);

    char *string = malloc(size + 1);
    assert_ptr(string);
    strncpy(string, start, size);
    string[size] = '\0';

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_STRING, 
            .inner.string = string,
            .range = (Range) {
                .start = start_position,
                .end = l->position,
            }
        },
    };
}

LexResult parse_number(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;
    bool has_decimal = false;

    char c = *start;
    for (;;) {

        next_char(l);
        c = *peek_char(l);
    }

    uint size = (uint)(l->str - start) + 1;

    char *ident = malloc(size + 1);
    assert_ptr(ident);
    strncpy(ident, start, size);
    ident[size] = '\0';

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_NUMBER, 
            .inner.number = 0.0,
            .range = (Range) {
                .start = start_position,
                .end = l->position,
            }
        },
    };
}

LexResult lex_next_tok(Lexer *l) {
    switch (*l->str) {
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        return parse_ident(l);
    case '"':
        return parse_string(l);
    case '0' ... '9':
        return parse_number(l);
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

#ifndef JAQ_TEST
#define jaq_assert(v, ...)                                                                         \
    if (!(v))                                                                                      \
        return false;
bool tok_equal(Token exp, Token actual) {
#else
#define jaq_assert(v, msg, args...)                                                                \
    if (!(v)) {                                                                                    \
        char *res = NULL;                                                                          \
        sprintf(res, msg, args);                                                                   \
        return res;                                                                                \
    };
char *tok_equal(Token exp, Token actual) {
#endif // JAQ_TEST
    jaq_assert(
        actual.type == exp.type, "Types not equal: Expected %d, got %d", exp.type, actual.type
    );
    switch (exp.type) {
    case TOKEN_IDENT:
        jaq_assert(
            strcmp(exp.inner.ident, actual.inner.ident) == 0,
            "inner.ident not equal: Expected %s, got %s",
            exp.inner.ident,
            actual.inner.ident
        );
        break;
    case TOKEN_STRING:
        jaq_assert(
            strcmp(exp.inner.string, actual.inner.string) == 0,
            "inner.string not equal: Expected %s, got %s",
            exp.inner.string,
            actual.inner.string
        );
        break;
    case TOKEN_NUMBER:
        jaq_assert(
            fabs(exp.inner.number - actual.inner.number) < 0.0001,
            "inner.number not equal: Expected %f, got %f",
            exp.inner.number,
            actual.inner.number
        );
        break;
    case TOKEN_KEYWORD:
        jaq_assert(
            exp.inner.keyword == actual.inner.keyword,
            "inner.keyword not equal: Expected %d, got %d",
            exp.inner.keyword,
            actual.inner.keyword
        );
        break;
    default:
        break;
    }

    jaq_assert(
        exp.range.start.col == actual.range.start.col
            && exp.range.start.line == actual.range.start.line
            && exp.range.end.col == actual.range.end.col
            && exp.range.end.line == actual.range.end.line,
        "range not equal: Expected (%d:%d-%d:%d), got (%d:%d-%d:%d)",

        exp.range.start.line,
        exp.range.start.col,
        exp.range.end.line,
        exp.range.end.col,
        actual.range.start.line,
        actual.range.start.col,
        actual.range.end.line,
        actual.range.end.col
    );

#ifdef JAQ_TEST
    return NULL;
#else
    return true;
#endif
}
#undef jaq_assert
