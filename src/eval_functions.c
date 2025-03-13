#include "src/errors.h"
#include "src/eval.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <stdio.h>
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
#define JSON_TYPE_ITERATOR (-3)

void free_eval_data(EvalData *e) {
    switch (e->type) {
    case SOME_ITER:
        iter_free(e->iter);
        break;
    case SOME_JSON:
        json_free(e->json);
        break;
    }
}

/// Assert that the arguments of the function call all match. In addition to all the normal
/// JsonTypes, you can also use JSON_TYPE_ANY and JSON_TYPE_CLOSURE.
///
/// `evaluated_args` should be pointer to an initially empty list. This function will evaluate each
/// ASTNode in `args`, provided the node isn't a closure.
///
/// This function will return the evaluated caller. For [1, 2].map(), the caller would be [1,2]
/// If this function causes an error, the evaluated caller should not be used because it will be
/// freed.
EvalData expect_function_args(
    Eval *e,
    char *function_name,
    ASTNode *function_node,
    Json *evaluated_args,
    JsonType expected_caller_type,
    JsonType *expected_types,
    uint length
) {
    if (eval_has_err(e)) {
        return eval_from_json(json_invalid());
    }
    ASTNode *caller_node = function_node->inner.function.callee;
    Vec_ASTNode args = function_node->inner.function.args;
    if (length != args.length) {
        eval_set_err(e, EVAL_ERR_FUNC_PARAM_NUMBER(function_name, length, args.length));
        return eval_from_json(json_invalid());
    }

    EvalData caller = eval_node(e, caller_node);

    if (expected_caller_type != JSON_TYPE_ANY && expected_caller_type != JSON_TYPE_ITERATOR) {
        Json jcaller = eval_to_json(e, caller);
        EXPECT_TYPE(
            e,
            jcaller,
            expected_caller_type,
            EVAL_ERR_FUNC_WRONG_CALLER(json_type(expected_caller_type), json_type(jcaller.type))
        );
        if (eval_has_err(e)) {
            json_free(jcaller);
            return eval_from_json(json_null());
        }

        // Cast back into an EvalData like this because when we casted caller into json we may have
        // converted an iterator into a list.
        caller = eval_from_json(jcaller);
    } else if (expected_caller_type == JSON_TYPE_ITERATOR) {
        JsonIterator icaller = eval_to_iter(e, caller);
        if (eval_has_err(e)) {
            if (icaller != NULL) {
                iter_free(icaller);
            }
            return eval_from_json(json_invalid());
        }

        // Cast back into an EvalData for the same reason as above.
        caller = eval_from_iter(icaller);
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
                free_eval_data(&caller);
                return eval_from_json(json_invalid());
            }
            break;
        default:
            if (args.data[i]->type == AST_TYPE_CLOSURE) {
                eval_set_err(e, EVAL_ERR_FUNC_UNEXPECTED_CLOSURE);

                // i - 1 here because we never evaluated this node
                _clean_up(i - 1, evaluated_args);
                free_eval_data(&caller);
                return eval_from_json(json_invalid());
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
                    free_eval_data(&caller);
                    return eval_from_json(json_invalid());
                }
                evaluated_args[i] = j;
                break;
            }
        }
    }

    return caller;
}

void vs_push_variable(VariableStack *vs, char *var_name, Json value) {
    vec_append(*vs, (Variable) {.value = value, .name = var_name});
}

void vs_pop_variable(VariableStack *vs, char *var_name) {
    Variable v = vec_pop(*vs);
    json_free(v.value);
    assert(strcmp(v.name, var_name) == 0);
}

Json vs_get_variable(Eval *e, char *var_name) {
    VariableStack vs = e->vs;
    for (uint i = vs.length - 1; i >= 0; i--) {
        if (strcmp(vs.data[i].name, var_name) == 0) {
            return json_copy(vs.data[i].value);
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

static JsonIterator eval_func_map(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};

    Vec_ASTNode args = node->inner.function.args;
    EvalData i = expect_function_args(
        e, "map", node, evaled_args, JSON_TYPE_ITERATOR, LIST((JsonType[]) {JSON_TYPE_CLOSURE})
    );
    if (eval_has_err(e)) {
        return NULL;
    }
    assert(args.data[0]->type == AST_TYPE_CLOSURE);

    // Safe to get iter here because we already made sure there were no errors
    JsonIterator iter = i.iter;

    struct map_closure *c = malloc(sizeof(*c));

    *c = (struct map_closure) {
        .e = e,
        .node = args.data[0]->inner.closure.body,
        // TODO: error checking here lol
        .param1_name = args.data[0]->inner.closure.args.data[0]->inner.primary.inner.ident,
    };

    return iter_map(iter, &mapper, c, true);
}

static Json eval_func_collect(Eval *e, ASTNode *node) {
    // TODO this should probably error if the type wasn't an iterator..
    return eval_to_json(e, eval_node(e, node->inner.function.callee));
}

static JsonIterator eval_func_iter(Eval *e, ASTNode *node) {
    // TODO this should probably error if the type wasn't json.
    return eval_to_iter(e, eval_node(e, node->inner.function.callee));
}

static JsonIterator eval_func_keys(Eval *e, ASTNode *node) {
    // Not expecting any arguments, hence the 0 length array
    Json evaled_args[0] = {};

    EvalData j = expect_function_args(
        e, "keys", node, evaled_args, JSON_TYPE_OBJECT, LIST((JsonType[]) {})
    );
    if (eval_has_err(e)) {
        return NULL;
    }

    Json json = j.json;
    assert(json.type == JSON_TYPE_OBJECT);
    return iter_obj_keys(json);
}

static JsonIterator eval_func_values(Eval *e, ASTNode *node) {
    // Not expecting any arguments, hence the 0 length array
    Json evaled_args[0] = {};

    EvalData j = expect_function_args(
        e, "values", node, evaled_args, JSON_TYPE_OBJECT, LIST((JsonType[]) {})
    );
    if (eval_has_err(e)) {
        return NULL;
    }

    Json json = j.json;
    assert(json.type == JSON_TYPE_OBJECT);
    return iter_obj_values(json);
}

EvalData eval_node_function(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    char *func_name = node->inner.function.function_name.inner.string;

    if (strcmp(func_name, "map") == 0) {
        return eval_from_iter(eval_func_map(e, node));
    } else if (strcmp(func_name, "collect") == 0) {
        return eval_from_json(eval_func_collect(e, node));
    } else if (strcmp(func_name, "iter") == 0) {
        return eval_from_iter(eval_func_iter(e, node));
    } else if (strcmp(func_name, "keys") == 0) {
        return eval_from_iter(eval_func_keys(e, node));
    } else if (strcmp(func_name, "values") == 0) {
        return eval_from_iter(eval_func_values(e, node));
    }

    eval_set_err(e, EVAL_ERR_FUNC_NOT_FOUND(func_name));
    BUBBLE_ERROR(e, (Json[]) {});
    unreachable("Bubbling would have returned the error");
}
