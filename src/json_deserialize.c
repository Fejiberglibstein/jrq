#include "src/errors.h"
#include "src/json.h"
#include "src/json_serde.h"
#include "src/lexer.h"
#include <memory.h>
#include <stdio.h>

#define LIST(v...) (v), (sizeof(v) / sizeof(*v))

static Json parse_json(Parser *p);
static Json parse_list(Parser *p);
static Json parse_object(Parser *p);

Json parse_object(Parser *p) {
    Json obj = json_object();

    if (p->curr.type != TOKEN_RBRACE) {
        do {
            parser_expect(p, TOKEN_STRING, ERROR_EXPECTED_STRING);
            if (p->error != NULL) {
                json_free(obj);
                return json_invalid();
            }

            Json key = json_string_from(p->prev.inner.string);

            parser_expect(p, TOKEN_COLON, ERROR_EXPECTED_COLON);
            if (p->error != NULL) {
                json_free(obj);
                json_free(key);
                return json_invalid();
            }

            Json value = parse_json(p);
            obj = json_object_set(obj, key, value);
        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    parser_expect(p, TOKEN_RBRACE, ERROR_MISSING_RBRACE);
    if (p->error != NULL) {
        json_free(obj);
        return json_invalid();
    }

    return obj;
}

Json parse_list(Parser *p) {
    Json j = json_list();

    if (p->curr.type != TOKEN_RBRACKET) {
        do {
            j = json_list_append(j, parse_json(p));
        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    parser_expect(p, TOKEN_RBRACKET, ERROR_MISSING_RBRACKET);

    if (p->error != NULL) {
        json_free(j);
        return json_invalid();
    }

    return j;
}

static Json parse_json(Parser *p) {
    // clang-format off
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_TRUE}))) return json_boolean(true);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_FALSE}))) return json_boolean(false);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_NULL}))) return json_null();

    if (parser_matches(p, LIST((TokenType[]) {TOKEN_LBRACE}))) return parse_object(p);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_LBRACKET}))) return parse_list(p);
    // clang-format on

    if (parser_matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_MINUS, TOKEN_NUMBER}))) {
        Token t = p->prev;

        switch (t.type) {
        case TOKEN_STRING:
            return json_string_from(t.inner.string);
        case TOKEN_NUMBER:
            return json_number(t.inner.number);
        case TOKEN_MINUS:
            parser_expect(p, TOKEN_NUMBER, "Invalid numerical literal");
            RETURN_ERR(p, json_invalid())
            t = p->prev;
            return json_number(-t.inner.number);

        default:
            // unreachable
            break;
        }
    }

    parser_expect(p, -1, ERROR_UNEXPECTED_TOKEN);
    RETURN_ERR(p, json_invalid())
    return (Json) {0};
}

DeserializeResult json_deserialize(char *str) {
    Lexer l = lex_init(str);

    Parser p = {
        .l = &l,
        .should_free = true,
    };

    parser_next(&p);
    Json j = parse_json(&p);
    parser_expect(&p, TOKEN_EOF, ERROR_EXPECTED_EOF);
    if (p.error != NULL) {
        tok_free(&p.curr);
        tok_free(&p.prev);
        json_free(j);
        return (DeserializeResult) {
            .err = jrq_error(p.curr.range, "%s", p.error),
            .type = RES_ERR,
        };
    }

    return (DeserializeResult) {.result = j};
}
