#include "src/eval.h"
#include "src/errors.h"
#include "src/json.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT_TYPE(j, _type, free_list, ...)                                                      \
    ({                                                                                             \
        Json __j = j;                                                                              \
        if (__j.type != _type) {                                                                   \
            uint __list_size = sizeof(free_list) / sizeof(*free_list);                             \
            for (uint i = 0; i < __list_size; i++) {                                               \
                json_free((free_list)[i]);                                                         \
            }                                                                                      \
                                                                                                   \
            return json_invalid_msg(TYPE_ERROR(__VA_ARGS__));                                      \
        }                                                                                          \
        __j;                                                                                       \
    })

#define PROPOGATE_INVALID(j, free_list...)                                                         \
    ({                                                                                             \
        Json __j = j;                                                                              \
        if (__j.type == JSON_TYPE_INVALID) {                                                       \
            uint __list_size = sizeof(free_list) / sizeof(*free_list);                             \
            for (uint i = 0; i < __list_size; i++) {                                               \
                json_free((free_list)[i]);                                                         \
            }                                                                                      \
                                                                                                   \
            return __j;                                                                            \
        }                                                                                          \
        __j;                                                                                       \
    })

struct Variable {
    char *name;
    Json value;
};
typedef Vec(struct Variable) VariableStack;

typedef struct {
    /// The input json from invoking the program
    Json input;

    /// The variables that are currently in scope.
    ///
    /// This is represented as a stack so that we can allow for variable
    /// shadowing:
    ///
    ///  [{"foo": 4}].map(|v| v.foo.map(|v| v))
    ///
    /// will create a stack like [v: {"foo": 4}, v: 4].
    VariableStack vars;
} Eval;

/// Will return a copy of the json value associated with the variable name
static Json get_variable(VariableStack *vs, char *var_name) {
    // Iterate through the stack in reverse order
    for (uint i = vs->length - 1; i >= 0; i--) {
        if (strcmp(vs->data[i].name, var_name) == 0) {
            return json_copy(vs->data[i].value);
        }
    }

    return json_invalid_msg(RUNTIME_ERROR("Variable not in scope: %s", var_name));
}

static Json eval_unary(Eval *e, ASTNode *node);
static Json eval_primary(Eval *e, ASTNode *node);
static Json eval_binary(Eval *e, ASTNode *node);
static Json eval_function(Eval *e, ASTNode *node);
static Json eval_access(Eval *e, ASTNode *node);
static Json eval_list(Eval *e, ASTNode *node);
static Json eval_json_object(Eval *e, ASTNode *node);
static Json eval_grouping(Eval *e, ASTNode *node);
static Json eval_node(Eval *e, ASTNode *node);

Json eval(ASTNode *node, Json input) {
    if (node == NULL) {
        return input;
    }

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
    case AST_TYPE_FALSE:
        return json_boolean(false);
    case AST_TYPE_TRUE:
        return json_boolean(true);
    case AST_TYPE_NULL:
        return json_null();

    case AST_TYPE_CLOSURE:
    case AST_TYPE_JSON_FIELD:
        // unreachable
        assert(false);
        break;
    }
}

static Json eval_primary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_PRIMARY);
    char *str;

    switch (node->inner.primary.type) {
    case TOKEN_STRING:
        str = node->inner.primary.inner.string;
        node->inner.primary.inner.string = NULL;
        return json_string_no_alloc(str);
    case TOKEN_IDENT:
        return get_variable(&e->vars, node->inner.primary.inner.ident);
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
        assert(false);
        break;
    }
}

static Json eval_access(Eval *e, ASTNode *node) {
}
static Json eval_function(Eval *e, ASTNode *node) {
}
static Json eval_list(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_LIST);

    Vec_ASTNode elems = node->inner.list;
    Json r = json_list_sized(elems.length);
    Json free_list[] = {r};

    for (int i = 0; i < elems.length; i++) {
        r = json_list_append(r, PROPOGATE_INVALID(eval_node(e, elems.data[i]), free_list));
    }

    return r;
}

static Json eval_json_object(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_JSON_OBJECT);

    Vec_ASTNode elems = node->inner.json_object;
    Json r = json_object_sized(elems.length);

    Json free_list[] = {r};

    for (int i = 0; i < elems.length; i++) {
        ASTNode *field = elems.data[i];
        assert(field->type == AST_TYPE_JSON_FIELD);

        Json key = PROPOGATE_INVALID(eval_node(e, field->inner.json_field.key), free_list);
        EXPECT_TYPE(
            key,
            JSON_TYPE_STRING,
            free_list,
            "Expected string for json key, got %s",
            json_type(key.type)
        );

        Json value = PROPOGATE_INVALID(
            eval_node(e, field->inner.json_field.value),
            (Json[]) {
                r,
                key,
            }
        );

        r = json_object_set(r, key, value);
    }

    return r;
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
        r = PROPOGATE_INVALID(eval_node(e, node->inner.unary.rhs), (Json[]) {});
        EXPECT_TYPE(
            r,
            JSON_TYPE_BOOL,
            (Json[]) {r},
            "Expected boolean in unary ! but got %s",
            json_type(r.type)
        );
        r.inner.boolean = !r.inner.boolean;
        return r;
    case TOKEN_MINUS:
        r = PROPOGATE_INVALID(eval_node(e, node->inner.unary.rhs), (Json[]) {});
        EXPECT_TYPE(
            r,
            JSON_TYPE_NUMBER,
            (Json[]) {r},
            "Expected number in unary - but got %s",
            json_type(r.type)
        );
        r.inner.number = -r.inner.number;
        return r;

    default:
        // Unreachable
        assert(false);
        break;
    }
}

static Json eval_binary(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_BINARY);

#define BINARY_OP(lhs, rhs, _type, op, _inner, _new)                                               \
    do {                                                                                           \
        (lhs) = eval_node(e, node->inner.binary.lhs);                                              \
        EXPECT_TYPE(                                                                               \
            (lhs),                                                                                 \
            _type,                                                                                 \
            (Json[]) {},                                                                           \
            "Invalid operands to binary " #op " (Expected %s, but got %s)",                        \
            json_type(_type),                                                                      \
            json_type((lhs).type)                                                                  \
        );                                                                                         \
        (rhs) = eval_node(e, node->inner.binary.rhs);                                              \
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
        lhs = eval_node(e, node->inner.binary.lhs);
        rhs = eval_node(e, node->inner.binary.rhs);

        ret = json_boolean(json_equal(lhs, rhs));
        json_free(lhs);
        json_free(rhs);
        return ret;
        break;
    case TOKEN_NOT_EQUAL:
        lhs = eval_node(e, node->inner.binary.lhs);
        rhs = eval_node(e, node->inner.binary.rhs);

        ret = json_boolean(!json_equal(lhs, rhs));
        json_free(lhs);
        json_free(rhs);
        return ret;
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
        lhs = eval_node(e, node->inner.binary.lhs);
        EXPECT_TYPE(
            lhs,
            JSON_TYPE_NUMBER,
            (Json[]) {},
            "Expected number in binary %% but got %s",
            json_type(lhs.type)
        );
        rhs = eval_node(e, node->inner.binary.rhs);
        EXPECT_TYPE(
            rhs,
            JSON_TYPE_NUMBER,
            (Json[]) {lhs},
            "Expected number in binary %% but got %s",
            json_type(rhs.type)
        );
        ret = json_number((int)lhs.inner.number % (int)rhs.inner.number);
        break;

    default:
        assert(false);
        break;
    }
    return ret;
#undef BINARY_OP
}
