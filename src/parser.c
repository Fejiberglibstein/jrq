#include "src/parser.h"
#include "src/errors.h"
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

#define LIST(v...) (v), (sizeof(v) / sizeof(*v))

static ASTNode *binary_factor(Parser *p);
static ASTNode *binary_term(Parser *p);
static ASTNode *binary_comparison(Parser *p);
static ASTNode *binary_equality(Parser *p);
static ASTNode *binary_logical_and(Parser *p);
static ASTNode *binary_logical_or(Parser *p);

static ASTNode *expression(Parser *p);
static ASTNode *unary(Parser *p);
static ASTNode *primary(Parser *p);

static ASTNode *access(Parser *p);
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

// clang-format off
PARSE_BINARY_OP(binary_factor, unary, {TOKEN_SLASH, TOKEN_ASTERISK});
PARSE_BINARY_OP(binary_term, binary_factor, {TOKEN_PLUS, TOKEN_MINUS});
PARSE_BINARY_OP(binary_comparison, binary_term, {TOKEN_LT_EQUAL, TOKEN_LANGLE, TOKEN_GT_EQUAL, TOKEN_RANGLE});
PARSE_BINARY_OP(binary_equality, binary_comparison, {TOKEN_EQUAL, TOKEN_NOT_EQUAL});
PARSE_BINARY_OP(binary_logical_and, binary_equality, {TOKEN_AND});
PARSE_BINARY_OP(binary_logical_or, binary_logical_and, {TOKEN_OR});
// clang-format on

static ASTNode *expression(Parser *p) {
    return binary_logical_or(p);
}

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
                vec_append(closure_args, expression(p));
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
            expect(p, TOKEN_IDENT, ERROR_EXPECTED_IDENT);
            Token ident = p->prev;

            ASTNode *new_expr = calloc(sizeof(ASTNode), 1);
            new_expr->type = AST_TYPE_ACCESS;
            new_expr->inner.access.ident = ident;
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
    if (matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_INT, TOKEN_DOUBLE, TOKEN_IDENT}))) {
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
    p->error = ERROR_UNEXPECTED_TOKEN;

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
