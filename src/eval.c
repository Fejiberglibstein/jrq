#include "src/eval.h"
#include "src/errors.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>

#define unreachable(str) assert(false && str)

// clang-format off
EvalData eval_from_json(Json j) { return (EvalData) {.type = SOME_JSON, .json = j}; }
EvalData eval_from_iter(JsonIterator i) { return (EvalData) {.type = SOME_ITER, .iter = i}; }
// clang-format on

Json eval_to_json(Eval *e, EvalData d) {
    if (d.type == SOME_JSON) {
        return d.json;
    }
    return iter_collect(d.iter);
}

JsonIterator eval_to_iter(Eval *e, EvalData d) {
    if (d.type == SOME_ITER) {
        return d.iter;
    }
    if (d.json.type == JSON_TYPE_LIST) {
        return iter_list(d.json);
    }
    eval_set_err(e, "Expected Iterator, got %s", json_type(d.json.type));
    return NULL;
}

static EvalData eval_node_primary(Eval *e, ASTNode *node);
static EvalData eval_node_unary(Eval *e, ASTNode *node);
static EvalData eval_node_binary(Eval *e, ASTNode *node);
static EvalData eval_node_grouping(Eval *e, ASTNode *node);
static EvalData eval_node_list(Eval *e, ASTNode *node);
static EvalData eval_node_json(Eval *e, ASTNode *node);
static EvalData eval_node_access(Eval *e, ASTNode *node);
static EvalData eval_node_function(Eval *e, ASTNode *node);
static EvalData eval_node_closure(Eval *e, ASTNode *node);

EvalData eval_node(Eval *e, ASTNode *node) {
    if (node == NULL) {
        return eval_from_json(e->input);
    }

    switch (node->type) {
    case AST_TYPE_PRIMARY:
        return eval_node_primary(e, node);
    case AST_TYPE_UNARY:
        return eval_node_unary(e, node);
    case AST_TYPE_BINARY:
        return eval_node_binary(e, node);
    case AST_TYPE_GROUPING:
        return eval_node_grouping(e, node);
    case AST_TYPE_LIST:
        return eval_node_list(e, node);
    case AST_TYPE_JSON_OBJECT:
        return eval_node_json(e, node);
    case AST_TYPE_ACCESS:
        return eval_node_access(e, node);
    case AST_TYPE_FUNCTION:
        return eval_node_function(e, node);
    case AST_TYPE_CLOSURE:
        return eval_node_closure(e, node);
    case AST_TYPE_TRUE:
        return eval_from_json(json_boolean(true));
    case AST_TYPE_FALSE:
        return eval_from_json(json_boolean(false));
    case AST_TYPE_NULL:
        return eval_from_json(json_null());
    case AST_TYPE_JSON_FIELD:
        // This is handled when we eval the json_object type
        unreachable("Json Field shouldn't be eval'd");
        break;
    }
}

static EvalData eval_node_primary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_PRIMARY);
    char *str;
    e->range = node->range;

    switch (node->inner.primary.type) {
    case TOKEN_STRING:
    case TOKEN_IDENT:
        // This is done to avoid a reallocation since the inner string is guaranteed to be heap
        // allocated.
        str = node->inner.primary.inner.string;
        node->inner.primary.inner.string = NULL;
        return eval_from_json(json_string_no_alloc(str));
    case TOKEN_NUMBER:
        return eval_from_json(json_number(node->inner.primary.inner.number));
    default:
        unreachable("All primary tokens covered, booleans and null are separate AST nodes");
        break;
    }
}

static EvalData eval_node_unary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_UNARY);

    Json j = eval_to_json(e, eval_node(e, node->inner.unary.rhs));

    switch (node->inner.unary.operator) {
    case TOKEN_MINUS:
        EXPECT_TYPE(e, j, JSON_TYPE_NUMBER, EVAL_ERR_UNARY_MINUS(json_type(j.type)));
        BUBBLE_ERROR(e, (Json[]) {j});

        e->range = node->range;
        j.inner.number = -j.inner.number;
        return eval_from_json(j);
    case TOKEN_BANG:
        EXPECT_TYPE(e, j, JSON_TYPE_BOOL, EVAL_ERR_UNARY_NOT(json_type(j.type)));
        BUBBLE_ERROR(e, (Json[]) {j});

        e->range = node->range;
        j.inner.boolean = !j.inner.boolean;
        return eval_from_json(j);
    default:
        unreachable("No other token is a unary operator");
    }
}

#define EVAL_BINARY_OP(_NAME, _EXPECTED_TYPE, _OP_NAME, _OUT_FUNCTION)                             \
    static EvalData _NAME(Eval *e, ASTNode *node) {                                                \
        assert(node->type == AST_TYPE_BINARY);                                                     \
                                                                                                   \
        Json lhs = eval_to_json(e, eval_node(e, node->inner.binary.lhs));                          \
        Json rhs = eval_to_json(e, eval_node(e, node->inner.binary.rhs));                          \
        EXPECT_TYPE(                                                                               \
            e,                                                                                     \
            lhs,                                                                                   \
            _EXPECTED_TYPE,                                                                        \
            EVAL_ERR_BINARY_OP(_OP_NAME, json_type(_EXPECTED_TYPE), json_type(lhs.type))           \
        );                                                                                         \
        EXPECT_TYPE(                                                                               \
            e,                                                                                     \
            rhs,                                                                                   \
            _EXPECTED_TYPE,                                                                        \
            EVAL_ERR_BINARY_OP(_OP_NAME, json_type(_EXPECTED_TYPE), json_type(rhs.type))           \
        );                                                                                         \
        BUBBLE_ERROR(e, (Json[]) {lhs, rhs});                                                      \
                                                                                                   \
        Json ret = _OUT_FUNCTION;                                                                  \
        json_free(lhs);                                                                            \
        json_free(rhs);                                                                            \
        return eval_from_json(ret);                                                                \
    }

// clang-format off
EVAL_BINARY_OP(eval_binary_or, JSON_TYPE_BOOL, "||", json_boolean(lhs.inner.boolean || rhs.inner.boolean));
EVAL_BINARY_OP(eval_binary_and, JSON_TYPE_BOOL, "&&", json_boolean(lhs.inner.boolean && rhs.inner.boolean));
EVAL_BINARY_OP(eval_binary_lt_equal, JSON_TYPE_NUMBER, "<=", json_boolean(lhs.inner.number <= rhs.inner.number));
EVAL_BINARY_OP(eval_binary_gt_equal, JSON_TYPE_NUMBER, ">=", json_boolean(lhs.inner.number >= rhs.inner.number));
EVAL_BINARY_OP(eval_binary_gt, JSON_TYPE_NUMBER, ">", json_boolean(lhs.inner.number > rhs.inner.number));
EVAL_BINARY_OP(eval_binary_lt, JSON_TYPE_NUMBER, "<", json_boolean(lhs.inner.number < rhs.inner.number));
EVAL_BINARY_OP(eval_binary_add, JSON_TYPE_NUMBER, "+", json_number(lhs.inner.number + rhs.inner.number));
EVAL_BINARY_OP(eval_binary_sub, JSON_TYPE_NUMBER, "-", json_number(lhs.inner.number - rhs.inner.number));
EVAL_BINARY_OP(eval_binary_times, JSON_TYPE_NUMBER, "*", json_number(lhs.inner.number * rhs.inner.number));
EVAL_BINARY_OP(eval_binary_div, JSON_TYPE_NUMBER, "/", json_number(lhs.inner.number / rhs.inner.number));
EVAL_BINARY_OP(eval_binary_mod, JSON_TYPE_NUMBER, "%%", json_number(fmod(lhs.inner.number, rhs.inner.number)));
// clang-format on

static EvalData eval_node_binary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_BINARY);

    Json lhs = eval_to_json(e, eval_node(e, node->inner.binary.lhs));
    Json rhs = eval_to_json(e, eval_node(e, node->inner.binary.rhs));
    BUBBLE_ERROR(e, (Json[]) {lhs, rhs});
    switch (node->inner.binary.operator) {
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
        bool b = json_equal(lhs, rhs);
        json_free(lhs);
        json_free(rhs);

        b = (node->inner.binary.operator== TOKEN_EQUAL) ? b : !b;
        return eval_from_json(json_boolean(b));
    case TOKEN_OR:
        return eval_binary_or(e, node);
    case TOKEN_AND:
        return eval_binary_and(e, node);
    case TOKEN_LT_EQUAL:
        return eval_binary_lt_equal(e, node);
    case TOKEN_GT_EQUAL:
        return eval_binary_gt_equal(e, node);
    case TOKEN_RANGLE:
        return eval_binary_gt(e, node);
    case TOKEN_LANGLE:
        return eval_binary_lt(e, node);
    case TOKEN_PLUS:
        return eval_binary_add(e, node);
    case TOKEN_MINUS:
        return eval_binary_sub(e, node);
    case TOKEN_ASTERISK:
        return eval_binary_times(e, node);
    case TOKEN_SLASH:
        return eval_binary_div(e, node);
    case TOKEN_PERC:
        return eval_binary_mod(e, node);
    default:
        unreachable("No other token is a binary operator");
    }
}

EvalResult eval(ASTNode *node, Json input) {
    Eval e = (Eval) {
        .input = input,
        .err = {0},
    };

    EvalData j = eval_node(&e, node);
    if (e.err.err != NULL) {
        return (EvalResult) {.err = e.err, .type = EVAL_ERR};
    } else {
        return (EvalResult) {.json = eval_to_json(&e, j)};
    }
}
