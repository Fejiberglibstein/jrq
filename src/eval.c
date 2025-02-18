#include "src/eval.h"
#include "src/json.h"
#include "src/parser.h"
#include <assert.h>
#include <stdint.h>

static Json eval_primary(ASTNode *node, Json input);
static Json eval_unary(ASTNode *node, Json input);
static Json eval_binary(ASTNode *node, Json input);
static Json eval_function(ASTNode *node, Json input);
static Json eval_access(ASTNode *node, Json input);
static Json eval_list(ASTNode *node, Json input);
static Json eval_json_field(ASTNode *node, Json input);
static Json eval_json_object(ASTNode *node, Json input);
static Json eval_grouping(ASTNode *node, Json input);

Json eval(ASTNode *node, Json input) {
    switch (node->type) {
    case AST_TYPE_PRIMARY:
        return eval_primary(node, input);
    case AST_TYPE_UNARY:
        return eval_unary(node, input);
    case AST_TYPE_BINARY:
        return eval_binary(node, input);
    case AST_TYPE_FUNCTION:
        return eval_function(node, input);
    case AST_TYPE_ACCESS:
        return eval_access(node, input);
    case AST_TYPE_LIST:
        return eval_list(node, input);
    case AST_TYPE_JSON_OBJECT:
        return eval_json_object(node, input);
    case AST_TYPE_GROUPING:
        return eval_grouping(node, input);
    default:
        assert(false);
        break;
    }
}

static Json eval_primary(ASTNode *node, Json _) {
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

static Json eval_grouping(ASTNode *node, Json input) {
    assert(node->type == AST_TYPE_GROUPING);

    return eval(node->inner.grouping, input);
}

static Json eval_unary(ASTNode *node, Json input) {
    assert(node->type == AST_TYPE_UNARY);

    switch (node->inner.unary.operator) {
        // TODO
    case TOKEN_BANG:
        return eval(node->inner.unary.rhs, input);
        // TODO
    case TOKEN_MINUS:
        return eval(node->inner.unary.rhs, input);

    default:
        // unreachable
        break;
    }
}

static Json eval_binary(ASTNode *node, Json input);
static Json eval_function(ASTNode *node, Json input);
static Json eval_access(ASTNode *node, Json input);
static Json eval_list(ASTNode *node);
static Json eval_json_object(ASTNode *node);
