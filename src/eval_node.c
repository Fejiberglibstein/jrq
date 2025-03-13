#include "src/errors.h"
#include "src/eval.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/json_serde.h"
#include "src/parser.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static EvalData eval_node_primary(Eval *e, ASTNode *node);
static EvalData eval_node_unary(Eval *e, ASTNode *node);
static EvalData eval_node_binary(Eval *e, ASTNode *node);
static EvalData eval_node_grouping(Eval *e, ASTNode *node);
static EvalData eval_node_list(Eval *e, ASTNode *node);
static EvalData eval_node_json(Eval *e, ASTNode *node);
static EvalData eval_node_access(Eval *e, ASTNode *node);
// static EvalData eval_node_closure(Eval *e, ASTNode *node);

EvalData eval_node(Eval *e, ASTNode *node) {
    if (node == NULL) {
        // TODO don't copy this all the time lol. maybe add a flag to json to not free it?
        return eval_from_json(json_copy(e->input));
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
    case AST_TYPE_TRUE:
        return eval_from_json(json_boolean(true));
    case AST_TYPE_FALSE:
        return eval_from_json(json_boolean(false));
    case AST_TYPE_NULL:
        return eval_from_json(json_null());
    case AST_TYPE_JSON_FIELD:
        // This is handled when we eval the json_object type
        unreachable("Json Field shouldn't be eval'd");
    case AST_TYPE_CLOSURE:
        unreachable("We shouldn't be evaluating closures");
    }
}

static EvalData eval_node_access(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_ACCESS);

    ASTNode *inner_node = node->inner.access.inner;
    Json inner = eval_to_json(e, eval_node(e, inner_node));

    bool used_input = inner_node == NULL;

    // // TODO
    // if (!used_input && inner_node->type == AST_TYPE_PRIMARY
    //     && inner_node->inner.primary.type == TOKEN_IDENT) {
    //     char *var_name = inner_node->inner.primary.inner.ident;
    //     json_free(inner);
    //     inner = vs_get_variable(e, var_name);
    // }

    Json accessor = eval_to_json(e, eval_node(e, node->inner.access.accessor));

    Json res = json_null();

    Json free_list[] = {inner, accessor};
    switch (inner.type) {
    case JSON_TYPE_LIST:
        EXPECT_TYPE(e, accessor, JSON_TYPE_NUMBER, EVAL_ERR_LIST_ACCESS(json_type(accessor.type)));
        BUBBLE_ERROR(e, free_list);

        res = json_copy(json_list_get(inner, (uint)accessor.inner.number));
        break;
    case JSON_TYPE_OBJECT:
        EXPECT_TYPE(e, accessor, JSON_TYPE_STRING, EVAL_ERR_JSON_ACCESS(json_type(accessor.type)));
        BUBBLE_ERROR(e, free_list);

        res = json_copy(json_object_get(&inner, accessor.inner.string));
        break;
    default:
        EXPECT_TYPE(e, inner, JSON_TYPE_LIST, EVAL_ERR_INNER_ACCESS(json_type(inner.type)));
        BUBBLE_ERROR(e, free_list);
    }

    json_free(accessor);
    json_free(inner);

    e->range = node->range;
    return eval_from_json(res);
}

static EvalData eval_node_json(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_JSON_OBJECT);

    Vec_ASTNode elems = node->inner.json_object;
    Json obj = json_object_sized(elems.length);

    for (int i = 0; i < elems.length; i++) {
        ASTNode *field = elems.data[i];
        assert(field->type == AST_TYPE_JSON_FIELD);

        Json key = eval_to_json(e, eval_node(e, field->inner.json_field.key));
        Json value = eval_to_json(e, eval_node(e, field->inner.json_field.value));
        EXPECT_TYPE(e, key, JSON_TYPE_STRING, EVAL_ERR_JSON_KEY_STRING(json_type(key.type)));
        BUBBLE_ERROR(e, (Json[]) {obj, key, value});

        obj = json_object_set(obj, key, value);
    }

    e->range = node->range;
    return eval_from_json(obj);
}

static EvalData eval_node_list(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_LIST);

    Vec_ASTNode elems = node->inner.list;
    Json list = json_list_sized(elems.length);

    for (int i = 0; i < elems.length; i++) {
        Json el = eval_to_json(e, eval_node(e, elems.data[i]));
        BUBBLE_ERROR(e, (Json[]) {list, el});

        list = json_list_append(list, el);
    }
    e->range = node->range;

    return eval_from_json(list);
}

static EvalData eval_node_grouping(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_GROUPING);

    EvalData ret = eval_node(e, node->inner.grouping);
    e->range = node->range;

    return ret;
}

static EvalData eval_node_primary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_PRIMARY);
    char *str;
    e->range = node->range;

    switch (node->inner.primary.type) {
    case TOKEN_IDENT:
        return eval_from_json(vs_get_variable(e, node->inner.primary.inner.ident));
    case TOKEN_STRING:
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

        j.inner.number = -j.inner.number;

        break;
    case TOKEN_BANG:
        EXPECT_TYPE(e, j, JSON_TYPE_BOOL, EVAL_ERR_UNARY_NOT(json_type(j.type)));
        BUBBLE_ERROR(e, (Json[]) {j});

        j.inner.boolean = !j.inner.boolean;

        break;
    default:
        unreachable("No other token is a unary operator");
    }

    e->range = node->range;
    return eval_from_json(j);
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
        e->range = node->range;                                                                    \
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

    switch (node->inner.binary.operator) {
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
        Json lhs = eval_to_json(e, eval_node(e, node->inner.binary.lhs));
        Json rhs = eval_to_json(e, eval_node(e, node->inner.binary.rhs));
        BUBBLE_ERROR(e, (Json[]) {lhs, rhs});

        bool b = json_equal(lhs, rhs);
        json_free(lhs);
        json_free(rhs);

        e->range = node->range;
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
    if (e.vs.data != NULL) {
        free(e.vs.data);
    }
    if (e.err.err != NULL) {
        return (EvalResult) {.err = e.err, .type = EVAL_ERR};
    } else {
        return (EvalResult) {.json = eval_to_json(&e, j)};
    }
}
