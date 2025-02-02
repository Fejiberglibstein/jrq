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
