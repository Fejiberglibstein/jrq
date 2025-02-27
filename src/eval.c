#include "src/eval.h"
#include "src/errors.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/lexer.h"
#include "src/parser.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EvalResult eval_unary(Eval *e, ASTNode *node);
static EvalResult eval_primary(Eval *e, ASTNode *node);
static EvalResult eval_binary(Eval *e, ASTNode *node);
static EvalResult eval_function(Eval *e, ASTNode *node);
static EvalResult eval_access(Eval *e, ASTNode *node);
static EvalResult eval_list(Eval *e, ASTNode *node);
static EvalResult eval_json_object(Eval *e, ASTNode *node);
static EvalResult eval_grouping(Eval *e, ASTNode *node);

EvalResult eval_res(Json j) {
    return (EvalResult) {.type = EVAL_JSON, .json = j};
}

EvalResult eval_res_error(Range r, char *format, ...) {
    va_list args;
    va_start(args, format);

    char *msg;
    vasprintf(&msg, format, args);
    va_end(args);

    return (EvalResult) {.type = EVAL_ERR, .error = msg};
}

EvalResult eval(ASTNode *node, Json input) {
    return eval_node(&(Eval) {.input = input, .vars = (VariableStack) {}}, node);
}

EvalResult eval_node(Eval *e, ASTNode *node) {
    if (node == NULL) {
        return eval_res(e->input);
    }

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
    case AST_TYPE_FALSE:
        return eval_res(json_boolean(false));
    case AST_TYPE_TRUE:
        return eval_res(json_boolean(true));
    case AST_TYPE_NULL:
        return eval_res(json_null());

    case AST_TYPE_CLOSURE:
    case AST_TYPE_JSON_FIELD:
        // unreachable
        assert(false);
        break;
    }
}

static EvalResult eval_primary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_PRIMARY);
    char *str;

    switch (node->inner.primary.type) {
    case TOKEN_STRING:
    case TOKEN_IDENT:
        str = node->inner.primary.inner.string;
        node->inner.primary.inner.string = NULL;
        return eval_res(json_string_no_alloc(str));
    case TOKEN_NUMBER:
        return eval_res(json_number(node->inner.primary.inner.number));
    case TOKEN_TRUE:
        return eval_res(json_boolean(true));
    case TOKEN_FALSE:
        return eval_res(json_boolean(false));
    case TOKEN_NULL:
        return eval_res(json_null());
    default:
        // Unreachable
        assert(false);
        break;
    }
}

static EvalResult eval_access(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_ACCESS);

    ASTNode *inner_node = node->inner.access.inner;
    Json inner = BUBBLE_ERROR(eval_node(e, inner_node), (Json[]) {});

    bool used_input = node->inner.access.inner == NULL;

    if (!used_input && inner_node->type == AST_TYPE_PRIMARY
        && inner_node->inner.primary.type == TOKEN_IDENT) {
        char *var_name = inner_node->inner.primary.inner.ident;
        inner = BUBBLE_ERROR(vs_get_variable(&e->vars, var_name, inner_node->range), (Json[]) {});

        json_free(inner);
    }

    Json accessor = BUBBLE_ERROR(eval_node(e, node->inner.access.accessor), (Json[]) {inner});
    Json free_list[] = {(!used_input) ? inner : json_null(), accessor};

    Json ret;

    switch (inner.type) {
    case JSON_TYPE_LIST:
        EXPECT_TYPE(
            node->inner.access.accessor->range,
            accessor,
            ((JsonType[]) {JSON_TYPE_NUMBER}),
            free_list,
            "Expected number in array access, got %s",
            json_type(accessor.type)
        );

        ret = json_copy(json_list_get(inner, (uint)accessor.inner.number));
        break;
    case JSON_TYPE_OBJECT:
        EXPECT_TYPE(
            node->inner.access.accessor->range,
            accessor,
            ((JsonType[]) {JSON_TYPE_STRING}),
            free_list,
            "Expected string in object access, got %s",
            json_type(accessor.type)
        );

        ret = json_copy(json_object_get(&inner, accessor.inner.string));
        break;

    default:
        // Unreachable
        assert(false);
    }

    if (!used_input) {
        json_free(inner);
    }
    json_free(accessor);

    return eval_res(ret);
}

static EvalResult eval_list(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_LIST);

    Vec_ASTNode elems = node->inner.list;
    Json r = json_list_sized(elems.length);
    Json free_list[] = {r};

    for (int i = 0; i < elems.length; i++) {
        r = json_list_append(
            r, EXPECT_JSON(elems.data[i]->range, eval_node(e, elems.data[i]), free_list)
        );
    }

    return eval_res(r);
}

static EvalResult eval_json_object(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_JSON_OBJECT);

    Vec_ASTNode elems = node->inner.json_object;
    Json r = json_object_sized(elems.length);

    Json free_list[] = {r};

    for (int i = 0; i < elems.length; i++) {
        ASTNode *field = elems.data[i];
        assert(field->type == AST_TYPE_JSON_FIELD);

        Json key = EXPECT_JSON(
            field->inner.json_field.key->range, eval_node(e, field->inner.json_field.key), free_list
        );
        EXPECT_TYPE(
            field->inner.json_field.key->range,
            key,
            ((JsonType[]) {JSON_TYPE_STRING}),
            free_list,
            "Expected string for json key, got %s",
            json_type(key.type)
        );

        Json value = EXPECT_JSON(
            field->inner.json_field.value->range,
            eval_node(e, field->inner.json_field.value),
            ((Json[]) {r, key})
        );

        r = json_object_set(r, key, value);
    }

    return eval_res(r);
}

static EvalResult eval_grouping(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_GROUPING);

    return eval_node(e, node->inner.grouping);
}

static EvalResult eval_unary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_UNARY);
    Json r;

    switch (node->inner.unary.operator) {
    case TOKEN_BANG:
        r = EXPECT_JSON(
            node->inner.unary.rhs->range, eval_node(e, node->inner.unary.rhs), (Json[]) {}
        );
        EXPECT_TYPE(
            node->inner.unary.rhs->range,
            r,
            ((JsonType[]) {JSON_TYPE_BOOL}),
            (Json[]) {r},
            "Expected boolean in unary ! but got %s",
            json_type(r.type)
        );
        r.inner.boolean = !r.inner.boolean;
        return eval_res(r);
    case TOKEN_MINUS:
        r = EXPECT_JSON(
            node->inner.unary.rhs->range, eval_node(e, node->inner.unary.rhs), (Json[]) {}
        );
        EXPECT_TYPE(
            node->inner.unary.rhs->range,
            r,
            ((JsonType[]) {JSON_TYPE_NUMBER}),
            (Json[]) {r},
            "Expected number in unary - but got %s",
            json_type(r.type)
        );
        r.inner.number = -r.inner.number;
        return eval_res(r);

    default:
        // Unreachable
        assert(false);
        break;
    }
}

#define EVAL_BINARY_OP(_name, _expected_type, _op_name, _out_function)                             \
    static EvalResult _name(Eval *e, ASTNode *node) {                                              \
        assert(node->type == AST_TYPE_BINARY);                                                     \
                                                                                                   \
        Json lhs = BUBBLE_ERROR(eval_node(e, node->inner.binary.lhs), (Json[]) {});                \
        EXPECT_TYPE(                                                                               \
            node->inner.binary.lhs->range,                                                         \
            lhs,                                                                                   \
            (JsonType[]) {_expected_type},                                                         \
            (Json[]) {},                                                                           \
            "Invalid lhs argument to binary " _op_name " (expected %s but got %s)",                \
            json_type(_expected_type),                                                             \
            json_type(lhs.type)                                                                    \
        );                                                                                         \
        Json rhs = BUBBLE_ERROR(eval_node(e, node->inner.binary.rhs), (Json[]) {});                \
        EXPECT_TYPE(                                                                               \
            node->inner.binary.rhs->range,                                                         \
            rhs,                                                                                   \
            (JsonType[]) {_expected_type},                                                         \
            (Json[]) {lhs},                                                                        \
            "Invalid rhs argument to binary " _op_name " (expected %s but got %s)",                \
            json_type(_expected_type),                                                             \
            json_type(lhs.type)                                                                    \
        );                                                                                         \
                                                                                                   \
        Json ret = _out_function;                                                                  \
        json_free(lhs);                                                                            \
        json_free(rhs);                                                                            \
        return eval_res(ret);                                                                      \
    }

// clang-format off
EVAL_BINARY_OP(eval_binary_or, JSON_TYPE_BOOL, "||", json_boolean(lhs.inner.boolean || rhs.inner.boolean));
EVAL_BINARY_OP(eval_binary_and, JSON_TYPE_BOOL, "&&", json_boolean(lhs.inner.boolean && rhs.inner.boolean));
EVAL_BINARY_OP(eval_binary_lt_equal, JSON_TYPE_BOOL, "<=", json_boolean(lhs.inner.number <= rhs.inner.number));
EVAL_BINARY_OP(eval_binary_gt_equal, JSON_TYPE_BOOL, ">=", json_boolean(lhs.inner.number >= rhs.inner.number));
EVAL_BINARY_OP(eval_binary_lt, JSON_TYPE_BOOL, ">", json_boolean(lhs.inner.number > rhs.inner.number));
EVAL_BINARY_OP(eval_binary_gt, JSON_TYPE_BOOL, "<", json_boolean(lhs.inner.number < rhs.inner.number));
EVAL_BINARY_OP(eval_binary_add, JSON_TYPE_NUMBER, "+", json_number(lhs.inner.number + rhs.inner.number));
EVAL_BINARY_OP(eval_binary_sub, JSON_TYPE_NUMBER, "-", json_number(lhs.inner.number - rhs.inner.number));
EVAL_BINARY_OP(eval_binary_times, JSON_TYPE_NUMBER, "*", json_number(lhs.inner.number * rhs.inner.number));
EVAL_BINARY_OP(eval_binary_div, JSON_TYPE_NUMBER, "/", json_number(lhs.inner.number / rhs.inner.number));
EVAL_BINARY_OP(eval_binary_mod, JSON_TYPE_NUMBER, "%%", json_number(fmod(lhs.inner.number, rhs.inner.number)));
// clang-format on

static EvalResult eval_binary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_BINARY);

    Json lhs;
    Json rhs;
    Json ret;

    switch (node->inner.binary.operator) {
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
        lhs = EXPECT_JSON(
            node->inner.binary.lhs->range, eval_node(e, node->inner.binary.lhs), (Json[]) {}
        );
        rhs = EXPECT_JSON(
            node->inner.binary.rhs->range, eval_node(e, node->inner.binary.rhs), (Json[]) {lhs}
        );

        bool r = json_equal(lhs, rhs);
        ret = json_boolean(node->inner.binary.operator== TOKEN_EQUAL ? r : !r);
        json_free(lhs);
        json_free(rhs);
        return eval_res(ret);
        break;
    case TOKEN_OR:
        ret = BUBBLE_ERROR(eval_binary_or(e, node), (Json[]) {});
        break;
    case TOKEN_AND:
        ret = BUBBLE_ERROR(eval_binary_and(e, node), (Json[]) {});
        break;
    case TOKEN_LT_EQUAL:
        ret = BUBBLE_ERROR(eval_binary_lt_equal(e, node), (Json[]) {});
        break;
    case TOKEN_GT_EQUAL:
        ret = BUBBLE_ERROR(eval_binary_gt_equal(e, node), (Json[]) {});
        break;
    case TOKEN_LANGLE:
        ret = BUBBLE_ERROR(eval_binary_lt(e, node), (Json[]) {});
        break;
    case TOKEN_RANGLE:
        ret = BUBBLE_ERROR(eval_binary_gt(e, node), (Json[]) {});
        break;
    case TOKEN_PLUS:
        ret = BUBBLE_ERROR(eval_binary_add(e, node), (Json[]) {});
        break;
    case TOKEN_MINUS:
        ret = BUBBLE_ERROR(eval_binary_sub(e, node), (Json[]) {});
        break;
    case TOKEN_ASTERISK:
        ret = BUBBLE_ERROR(eval_binary_times(e, node), (Json[]) {});
        break;
    case TOKEN_SLASH:
        ret = BUBBLE_ERROR(eval_binary_div(e, node), (Json[]) {});
        break;
    case TOKEN_PERC:
        ret = BUBBLE_ERROR(eval_binary_mod(e, node), (Json[]) {});
        break;

    default:
        assert(false);
        break;
    }
    return eval_res(ret);
#undef BINARY_OP
}

/*************
 * FUNCTIONS *
 *************/

static EvalResult eval_function(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    assert(node->inner.function.function_name.type == TOKEN_IDENT);

    char *function_name = node->inner.function.function_name.inner.ident;

    // TODO make this a trie maybe
    if (strcmp(function_name, "map") == 0) {
        return eval_function_map(e, node);
    }
    return eval_res_error(node->range, "TODO     Invalid function name");
}
