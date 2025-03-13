#include "src/errors.h"
#include "src/eval.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define LIST(v...) (v), (sizeof(v) / sizeof(*v))

struct map_closure {
    Eval *e;
    ASTNode *node;
    char *param1_name;
};

#define JSON_TYPE_ANY (-1)
#define JSON_TYPE_CLOSURE (-2)

/// Assert that the arguments of the function call all match. In addition to all the normal
/// JsonTypes, you can also use JSON_TYPE_ANY and JSON_TYPE_CLOSURE. `evaluated_args` should be
///
/// pointer to an initially empty list. This function will evaluate each ASTNode in `args`, provided
/// the node isn't a closure.
void expect_function_args(
    Eval *e,
    char *function_name,
    Vec_ASTNode args,
    Json *evaluated_args,
    JsonType *expected_types,
    uint length
) {
    if (eval_has_err(e)) {
        return;
    }
    if (length != args.length) {
        eval_set_err(e, EVAL_ERR_FUNC_PARAM_NUMBER(function_name, length, args.length));
        return;
    }

    for (int i = 0; i < length; i++) {
        Json j;
        switch (expected_types[i]) {
        case JSON_TYPE_CLOSURE:
            // If we expect a closure type, give the evaluated args a null. This way we can prevent
            // any UB happening if we need to free our evaluated args.
            evaluated_args[i] = json_null();
            // if we expect a closure, make sure we actually have a closure
            if (args.data[i]->type != AST_TYPE_CLOSURE) {
                eval_set_err(e, EVAL_ERR_FUNC_NO_CLOSURE);
                _clean_up(i, evaluated_args);
                return;
            }
            break;
        default:
            if (args.data[i]->type == AST_TYPE_CLOSURE) {
                eval_set_err(e, EVAL_ERR_FUNC_UNEXPECTED_CLOSURE);

                // i - 1 here because we never evaluated this node
                _clean_up(i - 1, evaluated_args);
                return;
            }
            j = eval_to_json(e, eval_node(e, args.data[i]));
            // Make sure that our types match as long as we don't have an any type
            if (expected_types[i] != JSON_TYPE_ANY) {
                EXPECT_TYPE(
                    e,
                    j,
                    expected_types[i],
                    EVAL_ERR_FUNC_WRONG_ARGS(json_type(expected_types[i]), json_type(j.type))
                );
                if (eval_has_err(e)) {
                    _clean_up(i, evaluated_args);
                    return;
                }
                evaluated_args[i] = j;
                break;
            }
        }
    }
}

void vs_push_variable(VariableStack *vs, char *var_name, Json value) {
    vec_append(*vs, (Variable) {.value = value, .name = var_name});
}

void vs_pop_variable(VariableStack *vs, char *var_name) {
    Variable v = vec_pop(*vs);
    assert(strcmp(v.name, var_name) == 0);
}

Json vs_get_variable(Eval *e, char *var_name) {
    VariableStack vs = e->vs;
    for (uint i = vs.length - 1; i >= 0; i--) {
        if (strcmp(vs.data[i].name, var_name) == 0) {
            return vs.data[i].value;
        }
    }

    eval_set_err(e, EVAL_ERR_VAR_NOT_FOUND(var_name));
    return json_null();
}

static Json mapper(Json j, void *aux) {
    struct map_closure *c = aux;

    vs_push_variable(&c->e->vs, c->param1_name, j);
    Json ret = eval_to_json(c->e, eval_node(c->e, c->node));
    vs_pop_variable(&c->e->vs, c->param1_name);

    return ret;
}

static Json eval_func_collect(Eval *e, ASTNode *node) {
    // TODO this should probably error if the type wasn't an iterator..
    return eval_to_json(e, eval_node(e, node->inner.function.callee));
}

static JsonIterator eval_func_map(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};
    Vec_ASTNode args = node->inner.function.args;

    expect_function_args(e, "map", args, evaled_args, LIST((JsonType[]) {JSON_TYPE_CLOSURE}));
    if (eval_has_err(e)) {
        return NULL;
    }
    assert(args.data[0]->type == AST_TYPE_CLOSURE);

    JsonIterator iter = eval_to_iter(e, eval_node(e, node->inner.function.callee));
    if (eval_has_err(e)) {
        return NULL;
    }

    struct map_closure *c = malloc(sizeof(*c));

    *c = (struct map_closure) {
        .e = e,
        .node = node->inner.closure.body,
        // TODO: error checking here lol
        .param1_name = args.data[0]->inner.closure.args.data[0]->inner.primary.inner.ident,
    };

    return iter_map(iter, &mapper, c, true);
}

EvalData eval_node_function(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    char *func_name = node->inner.function.function_name.inner.string;

    if (strcmp(func_name, "map") == 0) {
        return eval_from_iter(eval_func_map(e, node));
    } else if (strcmp(func_name, "collect") == 0) {
        return eval_from_json(eval_func_collect(e, node));
    }

    eval_set_err(e, EVAL_ERR_FUNC_NOT_FOUND(func_name));
    BUBBLE_ERROR(e, (Json[]) {});
    unreachable("Bubbling would have returned the error");
}
