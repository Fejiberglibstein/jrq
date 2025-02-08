#include "./json.h"
#include "src/lexer.h"
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

static Json parse_string(Lexer *l);
static Json parse_number(Lexer *l);
static Json parse_keyword(Lexer *l);
static Json parse_list(Lexer *l);
static Json parse_struct(Lexer *l);
static Json parse_json(Lexer *l);

// Will return a pointer to the root element in a json string.
// If the json string is not properly formatted, the function will return NULL.
//
// The pointer returned can be freed by *just* calling free(ptr). This function
// only creates one allocation.
Json *json_deserialize(char *str) {
    str = res.res;
    if (res.err != NULL) {
        fprintf(stderr, "jaq: Parse error: %s\n", res.err);
        free(data.buf.data);
        return NULL;
    }
    str = skip_whitespace(str);
    if (*str != '\0') {
        free(data.buf.data);
        return NULL;
    }

    allocate_json(&data);

    // Reset the length so that while parsing we can use it to store where to
    // place the next complex type (struct/lists)
    data.buf.length = 0;
    parse_json(start, &data, 0, 0, NULL);

    free(data.buf.data);
    return data.arena;
}
