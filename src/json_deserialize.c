#include "./json.h"
#include "src/errors.h"
#include "src/lexer.h"
#include "src/utils.h"
#include "vector.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define LIST(v...) (v), (sizeof(v) / sizeof(*v))

typedef struct {
    Lexer *l;
    Token curr;
    Token prev;
    char *error;
} Parser;

static Json parse_json(Parser *p);
static Json parse_list(Parser *p);
static Json parse_object(Parser *p);

static void next(Parser *p) {
    if (p->error != NULL) {
        return;
    }
    LexResult t = lex_next_tok(p->l);
    if (t.error_message != NULL) {
        p->error = t.error_message;
        return;
    }

    p->prev = p->curr;
    p->curr = t.token;
}

static bool matches(Parser *p, TokenType types[], int length) {
    if ((p)->error) {
        return false;
    }
    for (int i = 0; i < length; i++) {
        if (p->curr.type == types[i]) {
            next(p);
            return true;
        }
    }
    return false;
}

static void expect(Parser *p, TokenType expected, char *err) {
    if (p->curr.type == expected) {
        next(p);
        return;
    }
    p->error = err;
}

Json parse_object(Parser *p) {
    Json *items = calloc(4, sizeof(Json));
    assert_ptr(items);

    if (p->curr.type != TOKEN_RBRACE) {
        int list_length = 1;
        int list_capacity = 4;
        do {
            if (list_length >= list_capacity) {
                list_capacity *= 2;
                void *tmp = realloc(items, list_capacity * sizeof(Json));
                assert_ptr(tmp);
                items = tmp;
            }

            expect(p, TOKEN_STRING, ERROR_EXPECTED_STRING);
            char *key = p->prev.inner.string;

            expect(p, TOKEN_COLON, ERROR_EXPECTED_COLON);

            Json j = parse_json(p);
            j.field_name = key;

            items[list_capacity++] = j;
        } while (matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    expect(p, TOKEN_RBRACE, ERROR_MISSING_RBRACE);

    return (Json) {
        .type = JSONTYPE_OBJECT,
        .inner.object = items,
    };
}

Json parse_list(Parser *p) {
    Json *items = calloc(4, sizeof(Json));
    assert_ptr(items);

    if (p->curr.type != TOKEN_RBRACKET) {
        int list_length = 1;
        int list_capacity = 4;
        do {
            if (list_length >= list_capacity) {
                list_capacity *= 2;
                void *tmp = realloc(items, list_capacity * sizeof(Json));
                assert_ptr(tmp);
                items = tmp;
            }
            items[list_capacity++] = parse_json(p);
        } while (matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    expect(p, TOKEN_RBRACKET, ERROR_MISSING_RBRACKET);

    return (Json) {
        .type = JSONTYPE_LIST,
        .inner.list = items,
    };
}

static Json keyword(Parser *p, JsonType t, bool val) {
    Json n = (Json) {
        .type = t,
    };

    if (t == JSONTYPE_BOOL) {
        n.inner.boolean = val;
    }

    return n;
}

static Json parse_json(Parser *p) {
    // clang-format off
    if (matches(p, LIST((TokenType[]) {TOKEN_TRUE}))) return keyword(p, JSONTYPE_BOOL, true);
    if (matches(p, LIST((TokenType[]) {TOKEN_FALSE}))) return keyword(p, JSONTYPE_BOOL, false);
    if (matches(p, LIST((TokenType[]) {TOKEN_NULL}))) return keyword(p, JSONTYPE_NULL, -1);

    if (matches(p, LIST((TokenType[]) {TOKEN_LBRACE}))) return parse_list(p);
    if (matches(p, LIST((TokenType[]) {TOKEN_LBRACKET}))) return parse_object(p);
    // clang-format on

    if (matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_MINUS, TOKEN_NUMBER}))) {
        Token t = p->prev;

        switch (t.type) {
        case TOKEN_STRING:
            return (Json) {
                .type = JSONTYPE_STRING,
                .inner.string = t.inner.string,
            };
        case TOKEN_NUMBER:
            return (Json) {
                .type = JSONTYPE_NUMBER,
                .inner.number = t.inner.number,
            };
        case TOKEN_MINUS:
            expect(p, TOKEN_NUMBER, "Invalid numerical literal");
            t = p->prev;
            return (Json) {
                .type = JSONTYPE_NUMBER,
                .inner.number = -t.inner.number,
            };

        default:
            // unreachable
            break;
        }
    }

    expect(p, -1, ERROR_UNEXPECTED_TOKEN);
    return (Json) {0};
}

Json *json_deserialize(char *str) {
    Lexer l = lex_init(str);

    Parser p = {
        .l = &l,
    };

    next(&p);
    Json j = parse_json(&p);
    expect(&p, TOKEN_EOF, ERROR_EXPECTED_EOF);
    if (p.error != NULL) {
        return NULL;
    }

    Json *d = calloc(sizeof(Json), 1);
    *d = j;

    return d;
}
