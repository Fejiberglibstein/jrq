#include "src/lexer.h"
#include "../test.h"
#include <assert.h>

void foo(char *, int, int);

char *tok_equal(Token exp, Token actual) {
    jaq_assert(INT, exp, actual, .type);

    switch (exp.type) {
    case TOKEN_IDENT:
        jaq_assert(STRING, exp, actual, .inner.ident);
        break;
    case TOKEN_STRING:
        jaq_assert(STRING, exp, actual, .inner.string);
        break;
    case TOKEN_NUMBER:
        jaq_assert(INT, exp, actual, .inner.number);
        break;
    default:
        break;
    }

    __jaq_assert(
        exp.range.start.col == actual.range.start.col
            && exp.range.start.line == actual.range.start.line
            && exp.range.end.col == actual.range.end.col
            && exp.range.end.line == actual.range.end.line,
        ".range not equal: Expected (%d:%d-%d:%d), got (%d:%d-%d:%d)",

        exp.range.start.line,
        exp.range.start.col,
        exp.range.end.line,
        exp.range.end.col,
        actual.range.start.line,
        actual.range.start.col,
        actual.range.end.line,
        actual.range.end.col
    );

    return NULL;
}

#define LIST(v...) (v), (sizeof(v) / sizeof(*v))
#define EPSILON 0.0001

void lex(char *inp, Token *expected, int len) {
    Lexer l = lex_init(inp);

    for (int i = 0; i < len; i++) {
        LexResult res = lex_next_tok(&l);
        assert(!res.error_message);
        assert(res.token.type != TOKEN_EOF);
        Token tok = res.token;
        Token exp = expected[i];

        char *eq = tok_equal(exp, tok);
        if (eq != NULL) {
            printf("%s\n", eq);
            assert(false);
        }
    }
    LexResult res = lex_next_tok(&l);
    assert(res.token.type == TOKEN_EOF);
}

void test_simple_lex() {
    lex("h  hdhd \"fo\\\"o\" j",
        LIST((Token[]) {
            (Token) {
                .range = (Range) {
                    .start = (Position) {.col = 1, .line = 1},
                    .end = (Position) {.col = 1, .line = 1},
                },
                .type = TOKEN_IDENT,
                .inner.ident = "h",
            },
            (Token) {
                .range = (Range) {
                    .start = (Position) {.col = 4, .line = 1},
                    .end = (Position) {.col = 7, .line = 1},
                },
                .type = TOKEN_IDENT,
                .inner.ident = "hdhd",
            },
            (Token) {
                .range = (Range) {
                    .start = (Position) {.col = 9, .line = 1},
                    .end = (Position) {.col = 15, .line = 1},
                },
                .type = TOKEN_STRING,
                .inner.ident = "fo\\\"o",
            },
            (Token) {
                .range = (Range) {
                    .start = (Position) {.col = 17, .line = 1},
                    .end = (Position) {.col = 17, .line = 1},
                },
                .type = TOKEN_IDENT,
                .inner.ident = "j",
            },
        }));

    lex(" 10.221 - 1023459678", LIST((Token[]) {
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 2, .line = 1},
                .end = (Position) {.col = 7, .line = 1},
            },
            .type = TOKEN_NUMBER,
            .inner.number =10.221,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 9, .line = 1},
                .end = (Position) {.col = 9, .line = 1},
            },
            .type = TOKEN_MINUS,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 11, .line = 1},
                .end = (Position) {.col = 20, .line = 1},
            },
            .type = TOKEN_NUMBER,
            .inner.number = 1023459678,
        },
    }));

    lex("tghh == (hh)", LIST((Token[]) {
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 1, .line = 1},
                .end = (Position) {.col = 4, .line = 1},
            },
            .type = TOKEN_IDENT,
            .inner.string = "tghh",
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 6, .line = 1},
                .end = (Position) {.col = 7, .line = 1},
            },
            .type = TOKEN_EQUAL,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 9, .line = 1},
                .end = (Position) {.col = 9, .line = 1},
            },
            .type = TOKEN_LPAREN,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 10, .line = 1},
                .end = (Position) {.col = 11, .line = 1},
            },
            .type = TOKEN_IDENT,
            .inner.ident = "hh",
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 12, .line = 1},
                .end = (Position) {.col = 12, .line = 1},
            },
            .type = TOKEN_RPAREN,
        },
    }));
    lex( " 10    -2 ==  4.3 " , LIST((Token[]) {
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 2, .line = 1},
                .end = (Position) {.col = 3, .line = 1},
            },
            .type = TOKEN_NUMBER,
            .inner.number = 10,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 8, .line = 1},
                .end = (Position) {.col = 8, .line = 1},
            },
            .type = TOKEN_MINUS,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 9, .line = 1},
                .end = (Position) {.col = 9, .line = 1},
            },
            .type = TOKEN_NUMBER,
            .inner.number = 2,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 11, .line = 1},
                .end = (Position) {.col = 12, .line = 1},
            },
            .type = TOKEN_EQUAL,
        },
        (Token) {
            .range = (Range) {
                .start = (Position) {.col = 15, .line = 1},
                .end = (Position) {.col = 17, .line = 1},
            },
            .type = TOKEN_NUMBER,
            .inner.number = 4.3
        },
    }));
}

int main() {
    test_simple_lex();
}
