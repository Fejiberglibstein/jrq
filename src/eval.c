#include "src/eval.h"
#include "src/errors.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/parser.h"
#include <assert.h>
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

EvalResult eval_res_json(Json j) {
    return (EvalResult) {.type = EVAL_JSON, .json = j};
}

EvalResult eval_res_error(char *format, ...) {
    va_list args;
    va_start(args, format);

    char *msg;
    vasprintf(&msg, format, args);
    va_end(args);

    return (EvalResult) {.type = EVAL_ERR, .error = msg};
}

EvalResult eval_res_iter(JsonIterator iter) {
    return (EvalResult) {.type = EVAL_ITER, .iter = iter};
}

EvalResult eval(ASTNode *node, Json input) {
    return eval_node(&(Eval) {.input = input, .vars = (VariableStack) {}}, node);
}

EvalResult eval_node(Eval *e, ASTNode *node) {
    if (node == NULL) {
        return eval_res_json(e->input);
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
        return eval_res_json(json_boolean(false));
    case AST_TYPE_TRUE:
        return eval_res_json(json_boolean(true));
    case AST_TYPE_NULL:
        return eval_res_json(json_null());

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
        return eval_res_json(json_string_no_alloc(str));
    case TOKEN_NUMBER:
        return eval_res_json(json_number(node->inner.primary.inner.number));
    case TOKEN_TRUE:
        return eval_res_json(json_boolean(true));
    case TOKEN_FALSE:
        return eval_res_json(json_boolean(false));
    case TOKEN_NULL:
        return eval_res_json(json_null());
    default:
        // Unreachable
        assert(false);
        break;
    }
}

static EvalResult eval_access(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_ACCESS);

    ASTNode *inner_node = node->inner.access.inner;
    Json inner = EXPECT_JSON(eval_node(e, inner_node), (Json[]) {});

    bool used_input = node->inner.access.inner == NULL;

    if (!used_input && inner_node->type == AST_TYPE_PRIMARY
        && inner_node->inner.primary.type == TOKEN_IDENT) {
        char *var_name = inner_node->inner.primary.inner.ident;
        inner = vs_get_variable(&e->vars, var_name);

        json_free(inner);
    }

    Json accessor = EXPECT_JSON(eval_node(e, node->inner.access.accessor), (Json[]) {inner});
    Json free_list[] = {(!used_input) ? inner : json_null(), accessor};

    Json ret;

    switch (inner.type) {
    case JSON_TYPE_LIST:
        EXPECT_TYPE(
            accessor,
            JSON_TYPE_NUMBER,
            free_list,
            "Expected number in array access, got %s",
            json_type(accessor.type)
        );

        ret = json_copy(json_list_get(inner, (uint)accessor.inner.number));
        break;
    case JSON_TYPE_OBJECT:
        EXPECT_TYPE(
            accessor,
            JSON_TYPE_STRING,
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

    return eval_res_json(ret);
}

static EvalResult eval_list(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_LIST);

    Vec_ASTNode elems = node->inner.list;
    Json r = json_list_sized(elems.length);
    Json free_list[] = {r};

    for (int i = 0; i < elems.length; i++) {
        r = json_list_append(r, EXPECT_JSON(eval_node(e, elems.data[i]), free_list));
    }

    return eval_res_json(r);
}

static EvalResult eval_json_object(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_JSON_OBJECT);

    Vec_ASTNode elems = node->inner.json_object;
    Json r = json_object_sized(elems.length);

    Json free_list[] = {r};

    for (int i = 0; i < elems.length; i++) {
        ASTNode *field = elems.data[i];
        assert(field->type == AST_TYPE_JSON_FIELD);

        Json key = EXPECT_JSON(eval_node(e, field->inner.json_field.key), free_list);
        EXPECT_TYPE(
            key,
            JSON_TYPE_STRING,
            free_list,
            "Expected string for json key, got %s",
            json_type(key.type)
        );

        Json value = EXPECT_JSON(eval_node(e, field->inner.json_field.value), ((Json[]) {r, key}));

        r = json_object_set(r, key, value);
    }

    return eval_res_json(r);
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
        r = EXPECT_JSON(eval_node(e, node->inner.unary.rhs), (Json[]) {});
        EXPECT_TYPE(
            r,
            JSON_TYPE_BOOL,
            (Json[]) {r},
            "Expected boolean in unary ! but got %s",
            json_type(r.type)
        );
        r.inner.boolean = !r.inner.boolean;
        return eval_res_json(r);
    case TOKEN_MINUS:
        r = EXPECT_JSON(eval_node(e, node->inner.unary.rhs), (Json[]) {});
        EXPECT_TYPE(
            r,
            JSON_TYPE_NUMBER,
            (Json[]) {r},
            "Expected number in unary - but got %s",
            json_type(r.type)
        );
        r.inner.number = -r.inner.number;
        return eval_res_json(r);

    default:
        // Unreachable
        assert(false);
        break;
    }
}

static EvalResult eval_binary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_BINARY);

#define BINARY_OP(lhs, rhs, _type, op, _inner, _new)                                               \
    do {                                                                                           \
        (lhs) = EXPECT_JSON(eval_node(e, node->inner.binary.lhs), (Json[]) {});                    \
        EXPECT_TYPE(                                                                               \
            (lhs),                                                                                 \
            _type,                                                                                 \
            (Json[]) {},                                                                           \
            "Invalid operands to binary " #op " (Expected %s, but got %s)",                        \
            json_type(_type),                                                                      \
            json_type((lhs).type)                                                                  \
        );                                                                                         \
        (rhs) = EXPECT_JSON(eval_node(e, node->inner.binary.rhs), (Json[]) {lhs});                 \
        EXPECT_TYPE(                                                                               \
            (rhs),                                                                                 \
            _type,                                                                                 \
            (Json[]) {lhs},                                                                        \
            "Invalid operands to binary " #op " (Expected %s, but got %s)",                        \
            json_type(_type),                                                                      \
            json_type((rhs).type)                                                                  \
        );                                                                                         \
        ret = _new((lhs).inner._inner op rhs.inner._inner);                                        \
    } while (0)

    Json lhs;
    Json rhs;
    Json ret;

    switch (node->inner.binary.operator) {
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
        lhs = EXPECT_JSON(eval_node(e, node->inner.binary.lhs), (Json[]) {});
        rhs = EXPECT_JSON(eval_node(e, node->inner.binary.rhs), (Json[]) {lhs});

        bool r = json_equal(lhs, rhs);
        ret = json_boolean(node->inner.binary.operator== TOKEN_EQUAL ? r : !r);
        json_free(lhs);
        json_free(rhs);
        return eval_res_json(ret);
        break;
    case TOKEN_OR:
        BINARY_OP(lhs, rhs, JSON_TYPE_BOOL, ||, boolean, json_boolean);
        break;
    case TOKEN_AND:
        BINARY_OP(lhs, rhs, JSON_TYPE_BOOL, &&, boolean, json_boolean);
        break;
    case TOKEN_LT_EQUAL:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, <=, number, json_boolean);
        break;
    case TOKEN_GT_EQUAL:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, >=, number, json_boolean);
        break;
    case TOKEN_LANGLE:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, <, number, json_boolean);
        break;
    case TOKEN_RANGLE:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, >, number, json_boolean);
        break;
    case TOKEN_PLUS:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, +, number, json_number);
        break;
    case TOKEN_MINUS:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, -, number, json_number);
        break;
    case TOKEN_ASTERISK:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, *, number, json_number);
        break;
    case TOKEN_SLASH:
        BINARY_OP(lhs, rhs, JSON_TYPE_NUMBER, /, number, json_number);
        break;
    case TOKEN_PERC:
        lhs = EXPECT_JSON(eval_node(e, node->inner.binary.lhs), (Json[]) {});
        EXPECT_TYPE(
            (lhs),
            JSON_TYPE_NUMBER,
            (Json[]) {},
            "Invalid operands to binary %% (Expected number, but got %s)",
            json_type((lhs).type)
        );
        (rhs) = EXPECT_JSON(eval_node(e, node->inner.binary.rhs), (Json[]) {lhs});
        EXPECT_TYPE(
            (rhs),
            JSON_TYPE_NUMBER,
            (Json[]) {lhs},
            "Invalid operands to binary %% (Expected number, but got %s)",
            json_type((rhs).type)
        );
        ret = json_number((uint)lhs.inner.number % (uint)rhs.inner.number);
        break;

    default:
        assert(false);
        break;
    }
    return eval_res_json(ret);
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
    return eval_res_error("TODO     Invalid function name");
}
