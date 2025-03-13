#include "src/parser.h"
#include "src/errors.h"
#include "src/lexer.h"
#include <string.h>
#include <strings.h>

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

// "[" (expr ("," expr)*)? "]"
static ASTNode *list(Parser *p);

// "{" (json_field ("," json_field)*)? "]"
static ASTNode *json(Parser *p);

#define PARSE_BINARY_OP(_name_, _next_, _ops_...)                                                  \
    static ASTNode *_name_(Parser *p) {                                                            \
        Range start = p->curr.range;                                                               \
        ASTNode *expr = _next_(p);                                                                 \
                                                                                                   \
        while (parser_matches(p, LIST((TokenType[])_ops_))) {                                      \
            TokenType operator= p->prev.type;                                                      \
                                                                                                   \
            ASTNode *rhs = _next_(p);                                                              \
            ASTNode *new_expr = jrq_calloc(sizeof(ASTNode), 1);                                    \
                                                                                                   \
            new_expr->type = AST_TYPE_BINARY;                                                      \
            new_expr->inner.binary = (typeof(new_expr->inner.binary)) {                            \
                .lhs = expr,                                                                       \
                .operator= operator,                                                               \
                .rhs = rhs,                                                                        \
            };                                                                                     \
            expr = new_expr;                                                                       \
            expr->range = range_combine(start, p->prev.range);                                     \
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
PARSE_BINARY_OP(binary_factor, unary, {TOKEN_SLASH, TOKEN_ASTERISK, TOKEN_PERC});
// clang-format on

static ASTNode *unary(Parser *p) {
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_MINUS, TOKEN_BANG}))) {
        Range start = p->prev.range;
        TokenType operator= p->prev.type;
        ASTNode *rhs = unary(p);

        ASTNode *new_expr = jrq_calloc(sizeof(ASTNode), 1);

        new_expr->type = AST_TYPE_UNARY;
        new_expr->inner.unary = (typeof(new_expr->inner.unary)) {.rhs = rhs, .operator= operator, };

        new_expr->range = range_combine(start, p->prev.range);
        return new_expr;
    }
    return access(p);
}

static ASTNode *closure(Parser *p) {
    if (!parser_matches(p, LIST((TokenType[]) {TOKEN_BAR, TOKEN_OR}))) {
        return expression(p);
    }

    Range start = p->prev.range;
    Vec_ASTNode closure_args = (Vec_ASTNode) {0};

    // If we had a token_or "||" then there are no arguments to the closure.
    // On the other hand, if we had just a "|", then we have arguments in
    // the closure and must have a closing bar after the arguments.
    if (p->prev.type == TOKEN_BAR) {
        do {
            parser_expect(p, TOKEN_IDENT, ERROR_EXPECTED_IDENT);
            Token tok = p->prev;

            ASTNode *arg = jrq_calloc(sizeof(ASTNode), 1);
            arg->type = AST_TYPE_PRIMARY;
            arg->inner.primary = tok_norange(tok);

            vec_append(closure_args, arg);

        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
        parser_expect(p, TOKEN_BAR, ERROR_MISSING_CLOSURE);
    }
    ASTNode *closure_body = expression(p);

    ASTNode *closure = jrq_calloc(sizeof(ASTNode), 1);
    closure->type = AST_TYPE_CLOSURE;
    closure->inner.closure.args = closure_args;
    closure->inner.closure.body = closure_body;

    closure->range = range_combine(start, p->prev.range);

    return closure;
}

static ASTNode *function_call(Parser *p, ASTNode *callee, Token function_name) {
    Vec_ASTNode args = (Vec_ASTNode) {0};

    // If we don't have a closing paren immediately after the open paren
    if (p->curr.type != TOKEN_RPAREN) {
        do {
            vec_append(args, closure(p));
        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    parser_expect(p, TOKEN_RPAREN, ERROR_MISSING_RPAREN);

    ASTNode *function = jrq_calloc(sizeof(ASTNode), 1);
    function->type = AST_TYPE_FUNCTION;
    function->inner.function.args = args;
    function->inner.function.callee = callee;
    function->inner.function.function_name = tok_norange(function_name);
    // Don't update the function's range here, the range for a function is handled in `access`
    return function;
}

static ASTNode *access(Parser *p) {
    ASTNode *expr = (p->curr.type == TOKEN_DOT) ? NULL : primary(p);

    for (;;) {
        Range start = p->curr.range;
        if (parser_matches(p, LIST((TokenType[]) {TOKEN_DOT}))) {
            // When using the "." operator, we can access ONLY identifiers OR
            // numbers:
            //
            // {"foo": {"bar": 10}}.foo.bar - ok
            // [10, 2].1 - ok
            // [10, 48, 2].2.2 - technically ok, gets index 2
            // true.foo - ok, this can be parsed but won't be evaluated
            //
            // [10]."fooo" - not ok, its a string
            // {"foo": 10}.true - not ok, true is a keyword
            if (!parser_matches(p, LIST((TokenType[]) {TOKEN_NUMBER, TOKEN_IDENT}))) {
                // Expect -1 because this bit should never be happening
                parser_expect(p, -1, ERROR_EXPECTED_IDENT);
                return NULL;
            }
            Token ident = p->prev;

            // if we have a ( after the .expr
            if (parser_matches(p, LIST((TokenType[]) {TOKEN_LPAREN}))) {
                // TODO this is a really bad implementation for functions
                if (ident.type != TOKEN_IDENT) {
                    p->error = ERROR_EXPECTED_IDENT;
                } else {
                    expr = function_call(p, expr, ident);
                    expr->range = range_combine(start, p->prev.range);
                }
            } else {
                ASTNode *access = jrq_calloc(sizeof(ASTNode), 1);
                if (ident.type == TOKEN_IDENT) {
                    ident.type = TOKEN_STRING;
                }
                access->type = AST_TYPE_PRIMARY;
                access->inner.primary = tok_norange(ident);

                ASTNode *new_expr = jrq_calloc(sizeof(ASTNode), 1);
                new_expr->type = AST_TYPE_ACCESS;
                new_expr->inner.access.accessor = access;
                new_expr->inner.access.inner = expr;
                new_expr->range = range_combine(start, p->prev.range);

                expr = new_expr;
            }

        } else if (parser_matches(p, LIST((TokenType[]) {TOKEN_LBRACKET}))) {
            // When using the [] access operator, we can evaluate any expression
            // inside the [].
            ASTNode *access = expression(p);
            parser_expect(p, TOKEN_RBRACKET, ERROR_MISSING_RBRACKET);

            ASTNode *new_expr = jrq_calloc(sizeof(ASTNode), 1);
            new_expr->type = AST_TYPE_ACCESS;
            new_expr->inner.access.accessor = access;
            new_expr->inner.access.inner = expr;
            new_expr->range = range_combine(start, p->prev.range);

            expr = new_expr;
        } else {
            break;
        }
    }
    return expr;
}

static ASTNode *keyword(Parser *p, ASTNodeType t) {
    ASTNode *n = jrq_calloc(sizeof(ASTNode), 1);
    n->range = p->prev.range;

    n->type = t;
    return n;
}

static ASTNode *primary(Parser *p) {
    // clang-format off
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_TRUE}))) return keyword(p, AST_TYPE_TRUE);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_FALSE}))) return keyword(p, AST_TYPE_FALSE);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_NULL}))) return keyword(p, AST_TYPE_NULL);

    if (parser_matches(p, LIST((TokenType[]) {TOKEN_LBRACKET}))) return list(p);
    if (parser_matches(p, LIST((TokenType[]) {TOKEN_LBRACE}))) return json(p);
    // clang-format on

    if (parser_matches(p, LIST((TokenType[]) {TOKEN_STRING, TOKEN_NUMBER, TOKEN_IDENT}))) {
        ASTNode *new_expr = jrq_calloc(sizeof(ASTNode), 1);

        new_expr->type = AST_TYPE_PRIMARY;
        new_expr->inner.primary = tok_norange(p->prev);
        new_expr->range = p->prev.range;
        return new_expr;
    }

    if (parser_matches(p, LIST((TokenType[]) {TOKEN_LPAREN}))) {
        Range start = p->prev.range;

        ASTNode *expr = expression(p);
        parser_expect(p, TOKEN_RPAREN, ERROR_MISSING_RPAREN);

        ASTNode *grouping = jrq_calloc(sizeof(ASTNode), 1);

        grouping->type = AST_TYPE_GROUPING;
        grouping->inner.grouping = expr;
        grouping->range = range_combine(start, p->prev.range);

        return grouping;
    }

    if (p->error == NULL) {
        p->error = ERROR_UNEXPECTED_TOKEN;
    }

    return NULL;
}

static ASTNode *list(Parser *p) {
    Vec_ASTNode items = (Vec_ASTNode) {0};
    Range start = p->prev.range;

    // If we don't have a closing paren immediately after the open paren
    if (p->curr.type != TOKEN_RBRACKET) {
        do {
            vec_append(items, expression(p));
        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    parser_expect(p, TOKEN_RBRACKET, ERROR_MISSING_RBRACKET);

    ASTNode *list = jrq_calloc(sizeof(ASTNode), 1);
    list->type = AST_TYPE_LIST;
    list->inner.list = items;
    list->range = range_combine(start, p->prev.range);
    return list;
}

static ASTNode *json_field(Parser *p) {
    Range start = p->curr.range;

    ASTNode *key = access(p);

    parser_expect(p, TOKEN_COLON, ERROR_EXPECTED_COLON);

    ASTNode *value = expression(p);

    ASTNode *res = jrq_calloc(sizeof(ASTNode), 1);
    res->type = AST_TYPE_JSON_FIELD;
    res->inner.json_field.key = key;
    res->inner.json_field.value = value;
    res->range = range_combine(start, p->prev.range);

    return res;
}

static ASTNode *json(Parser *p) {
    Vec_ASTNode fields = (Vec_ASTNode) {0};
    Range start = p->prev.range;

    // If we don't have a closing paren immediately after the open paren
    if (p->curr.type != TOKEN_RBRACE) {
        do {
            vec_append(fields, json_field(p));
        } while (parser_matches(p, LIST((TokenType[]) {TOKEN_COMMA})));
    }

    parser_expect(p, TOKEN_RBRACE, ERROR_MISSING_RBRACE);

    ASTNode *json = jrq_calloc(sizeof(ASTNode), 1);
    json->type = AST_TYPE_JSON_OBJECT;
    json->inner.json_object = fields;
    json->range = range_combine(start, p->prev.range);

    return json;
}

ParseResult ast_parse(char *input) {
    Lexer l = lex_init(input);

    Parser *p = &(Parser) {
        .l = &l,
        .should_free = false,
    };

    parser_next(p);
    if (p->curr.type == TOKEN_EOF) {
        return (ParseResult) {.node = NULL};
    }

    ASTNode *node = expression(p);

    if (p->error != NULL) {
        ast_free(node);
        return (ParseResult) {.error_message = p->error};
    } else {
        // If we don't already have an error, make sure that the last token is
        // an EOF and then error if it's not
        parser_expect(p, TOKEN_EOF, ERROR_EXPECTED_EOF);
        if (p->error != NULL) {
            ast_free(node);
            return (ParseResult) {.error_message = p->error};
        }
        return (ParseResult) {.node = node};
    }
}

static void ast_vec_free(Vec_ASTNode vec) {
    for (int i = 0; i < vec.length; i++) {
        ast_free(vec.data[i]);
    }

    free((ASTNode *)vec.data);
}

void ast_free(ASTNode *n) {
    if (n == NULL) {
        return;
    }
    switch (n->type) {
    case AST_TYPE_PRIMARY:
        tok_free((Token *)&n->inner.primary);
        break;
    case AST_TYPE_UNARY:
        ast_free(n->inner.unary.rhs);
        break;
    case AST_TYPE_BINARY:
        ast_free(n->inner.binary.rhs);
        ast_free(n->inner.binary.lhs);
        break;
    case AST_TYPE_FUNCTION:
        ast_free(n->inner.function.callee);
        ast_vec_free(n->inner.function.args);
        tok_free((Token *)&n->inner.function.function_name);
        break;
    case AST_TYPE_CLOSURE:
        ast_free(n->inner.closure.body);
        ast_vec_free(n->inner.closure.args);
        break;
    case AST_TYPE_ACCESS:
        ast_free(n->inner.access.inner);
        ast_free(n->inner.access.accessor);
        break;
    case AST_TYPE_LIST:
        ast_vec_free(n->inner.list);
        break;
    case AST_TYPE_JSON_FIELD:
        ast_free(n->inner.json_field.key);
        ast_free(n->inner.json_field.value);
        break;
    case AST_TYPE_JSON_OBJECT:
        ast_vec_free(n->inner.json_object);
        break;
    case AST_TYPE_GROUPING:
        ast_free(n->inner.grouping);
        break;
    case AST_TYPE_FALSE:
    case AST_TYPE_TRUE:
    case AST_TYPE_NULL:
        break;
    }

    free(n);
}
