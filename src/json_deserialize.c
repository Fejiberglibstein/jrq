#include "src/errors.h"
#include "src/json.h"
#include "src/json_serde.h"
#include "src/lexer.h"
#include "src/vector.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define LIST(v...) (v), (sizeof(v) / sizeof(*v))

static Json parse_json(Parser *p);
static Json parse_list(Parser *p);
static Json parse_object(Parser *p);

Json parse_object(Parser *p) {
    Json obj = {
        .type = JSON_TYPE_OBJECT,
        .inner.object = {0},
    };

    if (p->curr.type != TOKEN_RBRACE) {
        do {
            parser_expect(p, TOKEN_STRING, ERROR_EXPECTED_STRING);
            Json key = json_string(p->prev.inner.string);

            parser_expect(p, TOKEN_COLON, ERROR_EXPECTED_COLON);

            Json value = parse_json(p);
            obj = json_object_set(obj, key, value);
        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    parser_expect(p, TOKEN_RBRACE, ERROR_MISSING_RBRACE);

    return obj;
}

Json parse_list(Parser *p) {
    JsonList items = {0};

    if (p->curr.type != TOKEN_RBRACKET) {
        do {
            vec_append(items, parse_json(p));
        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    parser_expect(p, TOKEN_RBRACKET, ERROR_MISSING_RBRACKET);

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
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_TRUE}))) return keyword(p, JSON_TYPE_BOOL, true);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_FALSE}))) return keyword(p, JSON_TYPE_BOOL, false);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_NULL}))) return keyword(p, JSON_TYPE_NULL, -1);

    if (parser_matches(p, LIST((TokenType[]) {TOKEN_LBRACE}))) return parse_object(p);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_LBRACKET}))) return parse_list(p);
    // clang-format on

    if (parser_matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_MINUS, TOKEN_NUMBER}))) {
        Token t = p->prev;

        switch (t.type) {
        case TOKEN_STRING:
            return json_string(t.inner.string);
        case TOKEN_NUMBER:
            return json_number(t.inner.number);
        case TOKEN_MINUS:
            parser_expect(p, TOKEN_NUMBER, "Invalid numerical literal");
            t = p->prev;
            return json_number(-t.inner.number);

        default:
            // unreachable
            break;
        }
    }

    parser_expect(p, -1, ERROR_UNEXPECTED_TOKEN);
    return (Json) {0};
}

DeserializeResult json_deserialize(char *str) {
    Lexer l = lex_init(str);

    Parser p = {
        .l = &l,
    };

    parser_next(&p);
    Json j = parse_json(&p);
    parser_expect(&p, TOKEN_EOF, ERROR_EXPECTED_EOF);
    if (p.error != NULL) {
        tok_free(&p.curr);
        tok_free(&p.prev);
        json_free(j);
        return (DeserializeResult) {.error = p.error};
    }

    return (DeserializeResult) {.result = j};
}
