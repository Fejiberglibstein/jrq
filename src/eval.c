#include "src/eval.h"
#include "src/errors.h"
#include "src/json.h"
#include "src/parser.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
} VariableStack;

typedef struct {
    // The input json from invoking the program
    Json input;

    VariableStack vars;
} Eval;

#define EXPECT_TYPE(j, _type, ...)                                                                 \
    ({                                                                                             \
        Json __j = j;                                                                              \
        if (__j.type != _type) {                                                                   \
            int n = sprintf(NULL, ""__VA_ARGS__);                                                  \
            char *msg = jrq_calloc(sizeof(char), n + 1);                                           \
            sprintf(msg, ""__VA_ARGS__);                                                           \
            return json_invalid_msg(msg);                                                          \
        }                                                                                          \
        __j;                                                                                       \
    })

static Json eval_unary(Eval *e, ASTNode *node);
static Json eval_primary(Eval *e, ASTNode *node);
static Json eval_binary(Eval *e, ASTNode *node);
static Json eval_function(Eval *e, ASTNode *node);
static Json eval_access(Eval *e, ASTNode *node);
static Json eval_list(Eval *e, ASTNode *node);
static Json eval_json_field(Eval *e, ASTNode *node);
static Json eval_json_object(Eval *e, ASTNode *node);
static Json eval_grouping(Eval *e, ASTNode *node);
static Json eval_node(Eval *e, ASTNode *node);

Json eval(ASTNode *node, Json input) {
    return eval_node(&(Eval) {.input = input, .vars = (VariableStack) {}}, node);
}

Json eval_node(Eval *e, ASTNode *node) {
    switch (node->type) {
    case AST_TYPE_PRIMARY:
        return eval_primary(e, node);
    case AST_TYPE_UNARY:
        return eval_unary(e, node);
    case AST_TYPE_BINARY:
        return eval_binary(e, node);
    case AST_TYPE_FUNCTION:
        return eval_function(e, node);
    case AST_TYPE_ACCESS:
        return eval_access(e, node);
    case AST_TYPE_LIST:
        return eval_list(e, node);
    case AST_TYPE_JSON_OBJECT:
        return eval_json_object(e, node);
    case AST_TYPE_GROUPING:
        return eval_grouping(e, node);
    default:
        assert(false);
        break;
    }
}

static Json eval_primary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_PRIMARY);

    switch (node->inner.primary.type) {
    case TOKEN_STRING:
        return json_string_no_alloc(node->inner.primary.inner.string);
    case TOKEN_NUMBER:
        return json_number(node->inner.primary.inner.number);
    case TOKEN_TRUE:
        return json_boolean(true);
    case TOKEN_FALSE:
        return json_boolean(false);
    case TOKEN_NULL:
        return json_null();
    default:
        // Unreachable
        return json_invalid();
    }
}

static Json eval_grouping(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_GROUPING);

    return eval_node(e, node->inner.grouping);
}

static Json eval_unary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_UNARY);
    Json r;

    switch (node->inner.unary.operator) {
    case TOKEN_BANG:
        r = eval_node(e, node->inner.unary.rhs);
        EXPECT_TYPE(
            r,
            JSON_TYPE_BOOL,
            TYPE_ERROR("Expected boolean in unary ! but got %s", json_type(r.type))
        );
        r.inner.boolean = !r.inner.boolean;
        return r;
    case TOKEN_MINUS:
        r = eval_node(e, node->inner.unary.rhs);
        EXPECT_TYPE(
            r,
            JSON_TYPE_NUMBER,
            TYPE_ERROR("Expected number in unary - but got %s", json_type(r.type))
        );
        r.inner.number = -r.inner.number;
        return r;

    default:
        // unreachable
        return json_invalid();
    }
}

static Json eval_binary(Eval *e, ASTNode *node) {
}
static Json eval_function(Eval *e, ASTNode *node) {
}
static Json eval_access(Eval *e, ASTNode *node) {
}
static Json eval_list(Eval *e, ASTNode *node) {
}
static Json eval_json_object(Eval *e, ASTNode *node) {
}
