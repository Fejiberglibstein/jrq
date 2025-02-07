#include "src/parser.h"
#include "src/errors.h"
#include "src/lexer.h"
#include "src/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    Token curr;
    Token prev;
    Lexer *l;
    char *error;
} Parser;

#define LIST(v...) (v), (sizeof(v) / sizeof(*v))

// (binary_logical_or)
static ASTNode *expression(Parser *p);

// (binary_logical_and) ("||" (binary_logical_and))*
static ASTNode *binary_logical_or(Parser *p);

// (binary_equality) ("&&" (binary_equality))*
static ASTNode *binary_logical_and(Parser *p);

// (binary_comparison) ("==" | "!=" (binary_comparison))*
static ASTNode *binary_equality(Parser *p);

// (binary_term) (">" | "<" | ">=" | "<=" (binary_term))*
static ASTNode *binary_comparison(Parser *p);

// (binary_factor) ("+" | "-" (binary_factor))*
static ASTNode *binary_term(Parser *p);

// (unary) ("*" | "/" (unary))*
static ASTNode *binary_factor(Parser *p);

// ("-" | "!" unary) | access
static ASTNode *unary(Parser *p);

// (primary) (("." (ident | number)) | ( "(" (function_args) ")") | ("[" expr "]"))*
static ASTNode *access(Parser *p);

// (string | number | identifier | keyword | json | list)
static ASTNode *primary(Parser *p);

// "|" (ident ("," ident)*)? "|" expr
static ASTNode *closure(Parser *p);

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

#define PARSE_BINARY_OP(_name_, _next_, _ops_...)                                                  \
    static ASTNode *_name_(Parser *p) {                                                            \
        /* printf(#_name_ "  pre: %d\n", p->curr.type); */                                         \
        ASTNode *expr = _next_(p);                                                                 \
        /* printf(#_name_ "  post: %d\n", p->curr.type); */                                        \
                                                                                                   \
        while (matches(p, LIST((TokenType[])_ops_))) {                                             \
            /* printf(#_name_ "  inner: %d\n", p->curr.type); */                                   \
            TokenType operator= p->prev.type;                                                      \
                                                                                                   \
            ASTNode *rhs = _next_(p);                                                              \
            /* printf(#_name_ "  inner: %d\n", p->curr.type); */                                   \
            ASTNode *new_expr = calloc(sizeof(ASTNode), 1);                                        \
            assert_ptr(new_expr);                                                                  \
                                                                                                   \
            new_expr->type = AST_TYPE_BINARY;                                                      \
            new_expr->inner.binary = (typeof(new_expr->inner.binary)) {                            \
                .lhs = expr,                                                                       \
                .operator= operator,                                                               \
                .rhs = rhs,                                                                        \
            };                                                                                     \
            expr = new_expr;                                                                       \
        }                                                                                          \
        return expr;                                                                               \
    }

static ASTNode *expression(Parser *p) {
    return binary_logical_or(p);
}

// clang-format off
PARSE_BINARY_OP(binary_logical_or, binary_logical_and, {TOKEN_OR});
PARSE_BINARY_OP(binary_logical_and, binary_equality, {TOKEN_AND});
PARSE_BINARY_OP(binary_equality, binary_comparison, {TOKEN_EQUAL, TOKEN_NOT_EQUAL});
PARSE_BINARY_OP(binary_comparison, binary_term, {TOKEN_LT_EQUAL, TOKEN_LANGLE, TOKEN_GT_EQUAL, TOKEN_RANGLE});
PARSE_BINARY_OP(binary_term, binary_factor, {TOKEN_PLUS, TOKEN_MINUS});
PARSE_BINARY_OP(binary_factor, unary, {TOKEN_SLASH, TOKEN_ASTERISK});
// clang-format on

static ASTNode *unary(Parser *p) {
    if (matches(p, LIST((TokenType[]) {TOKEN_MINUS, TOKEN_BANG}))) {
        TokenType operator= p->prev.type;
        ASTNode *rhs = unary(p);

        ASTNode *new_expr = calloc(sizeof(ASTNode), 1);
        assert_ptr(new_expr);

        new_expr->type = AST_TYPE_UNARY;
        new_expr->inner.unary = (typeof(new_expr->inner.unary)) {.rhs = rhs, .operator= operator, };
        return new_expr;
    }
    return access(p);
}

static ASTNode *closure(Parser *p) {
    if (matches(p, LIST((TokenType[]) {TOKEN_BAR, TOKEN_OR}))) {
        Vec_ASTNode closure_args = (Vec_ASTNode) {0};

        // If we had a token_or "||" then there are no arguments to the closure.
        // On the other hand, if we had just a "|", then we have arguments in
        // the closure and must have a closing bar after the arguments.
        if (p->prev.type == TOKEN_BAR) {
            do {
                expect(p, TOKEN_IDENT, ERROR_EXPECTED_IDENT);
                Token tok = p->prev;

                ASTNode *arg = calloc(sizeof(ASTNode), 1);
                arg->type = AST_TYPE_PRIMARY;
                arg->inner.primary = tok;

                vec_append(closure_args, arg);

            } while (matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
            expect(p, TOKEN_BAR, ERROR_MISSING_CLOSURE);
        }
        ASTNode *closure_body = expression(p);

        ASTNode *closure = calloc(sizeof(ASTNode), 1);
        closure->type = AST_TYPE_CLOSURE;
        closure->inner.closure.args = closure_args;
        closure->inner.closure.body = closure_body;

        return closure;
    }

    return expression(p);
}

static ASTNode *function_call(Parser *p, ASTNode *callee) {
    Vec_ASTNode args = (Vec_ASTNode) {0};

    // If we don't have a closing paren immediately after the open paren
    if (p->curr.type != TOKEN_RPAREN) {
        do {
            vec_append(args, closure(p));
        } while (matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    expect(p, TOKEN_RPAREN, ERROR_MISSING_RPAREN);

    ASTNode *function = calloc(sizeof(ASTNode), 1);
    function->type = AST_TYPE_FUNCTION;
    function->inner.function.args = args;
    function->inner.function.callee = callee;
    return function;
}

static ASTNode *access(Parser *p) {
    ASTNode *expr = primary(p);

    for (;;) {
        if (matches(p, LIST((TokenType[]) {TOKEN_LPAREN}))) {
            expr = function_call(p, expr);
        } else if (matches(p, LIST((TokenType[]) {TOKEN_DOT}))) {
            // When using the "." operator, we can access ONLY identifiers OR
            // numbers:
            //
            // {"foo": {"bar": 10}}.foo.bar - ok
            // [10, 2].1 - ok
            // [10, 48, 2].2.2 - technically ok, gets index 2
            //
            // [10]."fooo" - not ok, its a string
            // {"foo": 10}.true - not ok, true is a keyword
            if (!matches(p, LIST((TokenType[]) {TOKEN_NUMBER, TOKEN_IDENT}))) {
                // Expect -1 because this bit should never be happening
                expect(p, -1, ERROR_EXPECTED_IDENT);
                return NULL;
            }
            Token ident = p->prev;
            ASTNode *inner = calloc(sizeof(ASTNode), 1);
            inner->type = AST_TYPE_PRIMARY;
            inner->inner.primary = ident;

            ASTNode *new_expr = calloc(sizeof(ASTNode), 1);
            new_expr->type = AST_TYPE_ACCESS;
            new_expr->inner.access.accessor = inner;
            new_expr->inner.access.inner = expr;

            expr = new_expr;

        } else if (matches(p, LIST((TokenType[]) {TOKEN_LBRACKET}))) {
            // When using the [] access operator, we can evaluate any expression
            // inside the [].
            ASTNode *inner = expression(p);
            expect(p, TOKEN_RBRACKET, ERROR_MISSING_RBRACKET);

            ASTNode *new_expr = calloc(sizeof(ASTNode), 1);
            new_expr->type = AST_TYPE_ACCESS;
            new_expr->inner.access.accessor = inner;
            new_expr->inner.access.inner = expr;

            expr = new_expr;
        } else {
            break;
        }
    }
    return expr;
}

static ASTNode *keyword(Parser *p, ASTNodeType t) {
    ASTNode *n = calloc(sizeof(ASTNode), 1);
    assert_ptr(n);

    n->type = t;
    return n;
}

static ASTNode *primary(Parser *p) {
    // printf("primary %d\n", p->curr.type);

    // clang-format off
    if (matches(p, LIST((TokenType[]) {TOKEN_TRUE}))) return keyword(p, AST_TYPE_TRUE);
    if (matches(p, LIST((TokenType[]) {TOKEN_FALSE}))) return keyword(p, AST_TYPE_FALSE);
    if (matches(p, LIST((TokenType[]) {TOKEN_NULL}))) return keyword(p, AST_TYPE_NULL);
    // clang-format on

    // printf(" primary %d\n", p->curr.type);
    if (matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_NUMBER, TOKEN_IDENT}))) {
        ASTNode *new_expr = calloc(sizeof(ASTNode), 1);
        assert_ptr(new_expr);

        new_expr->type = AST_TYPE_PRIMARY;
        new_expr->inner.primary = p->prev;
        return new_expr;
    }

    // printf("  primary %d\n", p->curr.type);
    if (matches(p, LIST((TokenType[]) {TOKEN_LPAREN}))) {
        ASTNode *expr = expression(p);
        expect(p, TOKEN_RPAREN, ERROR_MISSING_RPAREN);

        ASTNode *grouping = calloc(sizeof(ASTNode), 1);
        assert_ptr(grouping);

        grouping->type = AST_TYPE_GROUPING;
        grouping->inner.grouping = expr;

        return grouping;
    }

    // printf("   primary %d\n", p->curr.type);
    if (p->error == NULL) {
        p->error = ERROR_UNEXPECTED_TOKEN;
    }

    return NULL;
}

ParseResult ast_parse(Lexer *l) {
    Parser *p = &(Parser) {
        .l = l,
    };

    next(p);

    ASTNode *node = expression(p);

    if (p->error != NULL) {
        return (ParseResult) {.error_message = p->error};
    } else {
        // If we don't already have an error, make sure that the last token is
        // an EOF and then error if it's not
        expect(p, TOKEN_EOF, ERROR_UNEXPECTED_TOKEN);
        if (p->error != NULL) {
            return (ParseResult) {.error_message = p->error};
        }
        return (ParseResult) {.node = node};
    }
}
