#include "src/parser.h"
#include "src/errors.h"
#include "src/lexer.h"
#include "src/utils.h"
#include <math.h>
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

static ASTNode *binary_factor(Parser *p);
static ASTNode *binary_term(Parser *p);
static ASTNode *binary_comparison(Parser *p);
static ASTNode *binary_equality(Parser *p);
static ASTNode *binary_logical_and(Parser *p);
static ASTNode *binary_logical_or(Parser *p);

static ASTNode *expression(Parser *p);
static ASTNode *unary(Parser *p);
static ASTNode *primary(Parser *p);

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

static void expect(Parser *p, TokenType expected, char *err) {
    if (p->curr.type == expected) {
        next(p);
        return;
    }
    p->error = err;
}

#define PARSE_BINARY_OP(_name_, _next_, _ops_...)                                                  \
    static ASTNode *_name_(Parser *p) {                                                            \
        ASTNode *expr = _next_(p);                                                                 \
                                                                                                   \
        while (matches(p, LIST((TokenType[])_ops_))) {                                             \
            TokenType operator= p->prev.type;                                                      \
                                                                                                   \
            ASTNode *rhs = _next_(p);                                                              \
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
    return primary(p);
}

static ASTNode *keyword(Parser *p, ASTNodeType t) {
    ASTNode *n = calloc(sizeof(ASTNode), 1);
    assert_ptr(n);

    n->type = t;
    return n;
}

static ASTNode *primary(Parser *p) {
    // clang-format off
    if (matches(p, LIST((TokenType[]) {TOKEN_TRUE}))) return keyword(p, AST_TYPE_TRUE);
    if (matches(p, LIST((TokenType[]) {TOKEN_FALSE}))) return keyword(p, AST_TYPE_FALSE);
    if (matches(p, LIST((TokenType[]) {TOKEN_NULL}))) return keyword(p, AST_TYPE_NULL);
    // clang-format on

    if (matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_INT, TOKEN_DOUBLE, TOKEN_IDENT}))) {
        ASTNode *new_expr = calloc(sizeof(ASTNode), 1);
        assert_ptr(new_expr);

        new_expr->type = AST_TYPE_PRIMARY;
        new_expr->inner.primary = p->prev;
        return new_expr;
    }

    if (matches(p, LIST((TokenType[]) {TOKEN_LPAREN}))) {
        ASTNode *expr = expression(p);
        expect(p, TOKEN_RPAREN, ERROR_MISSING_RPAREN);

        ASTNode *grouping = calloc(sizeof(ASTNode), 1);
        assert_ptr(grouping);

        grouping->type = AST_TYPE_GROUPING;
        grouping->inner.grouping = expr;
    }

    // todo
    p->error = ERROR_UNEXPECTED_TOKEN;

    return NULL;
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
