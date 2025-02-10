#include "src/errors.h"
#include "src/json.h"
#include "src/json_serde.h"
#include "src/lexer.h"
#include "src/utils.h"
#include "src/vector.h"
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
    Json obj = {
        .type = JSON_TYPE_OBJECT,
        .inner.object = {0},
    };

    if (p->curr.type != TOKEN_RBRACE) {
        do {
            expect(p, TOKEN_STRING, ERROR_EXPECTED_STRING);
            char *key = p->prev.inner.string;

            expect(p, TOKEN_COLON, ERROR_EXPECTED_COLON);

            Json value = parse_json(p);
            json_object_set(&obj, key, value);
        } while (matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    expect(p, TOKEN_RBRACE, ERROR_MISSING_RBRACE);

    return obj;
}

Json parse_list(Parser *p) {
    JsonList items = {0};

    if (p->curr.type != TOKEN_RBRACKET) {
        do {
            vec_append(items, parse_json(p));
        } while (matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    expect(p, TOKEN_RBRACKET, ERROR_MISSING_RBRACKET);

    return (Json) {
        .type = JSON_TYPE_LIST,
        .inner.list = items,
    };
}

static Json keyword(Parser *p, JsonType t, bool val) {
    Json n = (Json) {
        .type = t,
    };

    if (t == JSON_TYPE_BOOL) {
        n.inner.boolean = val;
    }

    return n;
}

static Json parse_json(Parser *p) {
    // clang-format off
    if (matches(p, LIST((TokenType[]) {TOKEN_TRUE}))) return keyword(p, JSON_TYPE_BOOL, true);
    if (matches(p, LIST((TokenType[]) {TOKEN_FALSE}))) return keyword(p, JSON_TYPE_BOOL, false);
    if (matches(p, LIST((TokenType[]) {TOKEN_NULL}))) return keyword(p, JSON_TYPE_NULL, -1);

    if (matches(p, LIST((TokenType[]) {TOKEN_LBRACE}))) return parse_object(p);
    if (matches(p, LIST((TokenType[]) {TOKEN_LBRACKET}))) return parse_list(p);
    // clang-format on

    if (matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_MINUS, TOKEN_NUMBER}))) {
        Token t = p->prev;

        switch (t.type) {
        case TOKEN_STRING:
            return (Json) {
                .type = JSON_TYPE_STRING,
                .inner.string = t.inner.string,
            };
        case TOKEN_NUMBER:
            return (Json) {
                .type = JSON_TYPE_NUMBER,
                .inner.number = t.inner.number,
            };
        case TOKEN_MINUS:
            expect(p, TOKEN_NUMBER, "Invalid numerical literal");
            t = p->prev;
            return (Json) {
                .type = JSON_TYPE_NUMBER,
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

DeserializeResult json_deserialize(char *str) {
    Lexer l = lex_init(str);

    Parser p = {
        .l = &l,
    };

    next(&p);
    Json j = parse_json(&p);
    expect(&p, TOKEN_EOF, ERROR_EXPECTED_EOF);
    if (p.error != NULL) {
        return (DeserializeResult) {.error = p.error};
    }

    return (DeserializeResult) {.result = j};
}
