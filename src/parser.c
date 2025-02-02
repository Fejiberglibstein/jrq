#include "src/parser.h"
#include "src/lexer.h"
#include "src/utils.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Token curr;
    Token prev;
    Lexer *l;
    char *error;
} Parser;

// clang-format off
#define ERR(p) ({if ((p)->error) {return NULL;}})
// clang-format on
#define LIST(v...) (v), (sizeof(v) / sizeof(*v))

static void next(Parser *p) {
    LexResult t = lex_next_tok(p->l);
    if (t.error_message != NULL) {
        p->error = t.error_message;
        return;
    }

    p->prev = p->curr;
    p->curr = t.token;
}

static bool matches(Parser *p, TokenType types[], int length) {
    ERR(p);
    for (int i = 0; i < length; i++) {
        if (p->curr.type == types[i]) {
            next(p);
            return true;
        }
    }
    return false;
}

static ASTNode *expression(Parser *p) {
    ASTNode *expr = logical_or(p);

    while (matches(p, LIST((TokenType[]) {TOKEN_OR}))) {
        TokenType operator= p->prev.type;

        ASTNode *rhs = logical_or(p);
        ASTNode *new_expr = calloc(sizeof(ASTNode), 1);
        assert_ptr(new_expr);

        new_expr->type = AST_TYPE_BINARY;
        new_expr->inner.binary = (typeof (new_expr->inner.binary)){
            .lhs = expr,
            .operator = operator,
            .rhs = rhs
        };
        expr = new_expr;
    }
    return expr;
}

ParseResult ast_parse(Lexer *l) {
    Parser p = (Parser) {
        .l = l,
    };

    ASTNode *node = expression(&p);
    if (p.error != NULL) {
        return (ParseResult) {.error_message = p.error};
    } else {
        return (ParseResult) {.node = node};
    }
}
