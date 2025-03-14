#include "src/alloc.h"
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

#define JSON_TYPE_ANY (-1)
#define JSON_TYPE_ITERATOR (-2)
#define JSON_TYPE_CLOSURE (-10)
#define JSON_TYPE_CLOSURE_WITH_PARAMS(v) (JSON_TYPE_CLOSURE - (v))

/// Used to define what types a function expects
///
/// <number>.add(number)
/// The function_data for the previous function would have this form
/// {
///     .function_name    = "add",
///     .caller_type      = JSON_TYPE_NUMBER,
///     .parameter_types  = (JsonType[]) {JSON_TYPE_NUMBER},
///     .parameter_amount = 1,
/// }
///
/// In addition to all the normal json types, there is also the following json types you can use:
/// - JSON_TYPE_ANY
/// - JSON_TYPE_ITERATOR
/// - JSON_TYPE_CLOSURE                  (closure with no parameters) -> ||
/// - JSON_TYPE_CLOSURE_WITH_PARAMS(N)   (closure with N parameters)  -> |p1, p2, ..., pN|
struct function_data {
    /// The name of the function
    char *function_name;
    /// The expected type of the caller of the function.
    ///
    /// In `[10, 2].foo()`, the caller is of type JSON_TYPE_LIST
    JsonType caller_type;
    /// The expected type of each parameter to the function in a list.
    ///
    /// In `.foo(10, "grb")`, the parameter types would be [number, string]
    JsonType *parameter_types;
    /// The amount of parameters this function expects.
    uint parameter_amount;
};

static EvalData func_expect_args(Eval *, ASTNode *, Json *, struct function_data);

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

//
// BEGIN FUNCTIONS
//

struct map_closure {
    Eval *e;
    ASTNode *node;
    char *param1_name;
};

static Json mapper(Json j, void *aux) {
    struct map_closure *c = aux;

    vs_push_variable(&c->e->vs, c->param1_name, j);
    Json ret = eval_to_json(c->e, eval_node(c->e, c->node));
    vs_pop_variable(&c->e->vs, c->param1_name);

    return ret;
}

static struct function_data MAP_FUNC = {
    .function_name = "map",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_CLOSURE_WITH_PARAMS(1)},
    .parameter_amount = 1,
};
static JsonIterator eval_func_map(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};

    Vec_ASTNode args = node->inner.function.args;
    EvalData i = func_expect_args(e, node, evaled_args, MAP_FUNC);
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
        // TODO allow for |[foo, bar]| variable name types to work
        //   (tuple closure parameters, like in rust: `|(foo, bar)|`)

        // clang-format off
        .param1_name = args.data[0]->inner.closure.args.data[0] // the length of the closure was already checked, so this is fine to do.
              ->inner.primary.inner.ident, // TODO this is not what we want to do, see above todo comment
        // clang-format on
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

static struct function_data KEYS_FUNC = {
    .function_name = "keys",
    .caller_type = JSON_TYPE_OBJECT,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static JsonIterator eval_func_keys(Eval *e, ASTNode *node) {
    // Not expecting any arguments, hence the 0 length array
    Json evaled_args[0] = {};
    EvalData j = func_expect_args(e, node, evaled_args, KEYS_FUNC);
    if (eval_has_err(e)) {
        return NULL;
    }

    Json json = j.json;
    assert(json.type == JSON_TYPE_OBJECT);
    return iter_obj_keys(json);
}

static struct function_data VALUES_FUNC = {
    .function_name = "values",
    .caller_type = JSON_TYPE_OBJECT,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static JsonIterator eval_func_values(Eval *e, ASTNode *node) {
    // Not expecting any arguments, hence the 0 length array
    Json evaled_args[0] = {};

    EvalData j = func_expect_args(e, node, evaled_args, VALUES_FUNC);
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

static EvalData func_eval_caller(Eval *e, ASTNode *function_node, struct function_data func) {
    EvalData err = eval_from_json(json_invalid());

    EvalData caller = eval_node(e, function_node->inner.function.callee);

    if (func.caller_type != JSON_TYPE_ITERATOR && func.caller_type != JSON_TYPE_ANY) {
        // cast caller into json
        Json jcaller = eval_to_json(e, caller);
        EXPECT_TYPE(
            e,
            jcaller,
            func.caller_type,
            EVAL_ERR_FUNC_WRONG_CALLER(json_type(func.caller_type), json_type(jcaller.type))
        );
        if (eval_has_err(e)) {
            json_free(jcaller);
            return err;
        }

        // Cast back into an EvalData like this because when we casted caller into json we may have
        // converted an iterator into a list.
        caller = eval_from_json(jcaller);
    } else if (func.caller_type == JSON_TYPE_ITERATOR) {
        JsonIterator icaller = eval_to_iter(e, caller);
        if (eval_has_err(e)) {
            if (icaller != NULL) {
                iter_free(icaller);
            }
            return err;
        }

        // Cast back into an EvalData for the same reason as above.
        caller = eval_from_iter(icaller);
    }
    return caller;
}

static void func_eval_params(
    Eval *e,
    ASTNode *function_node,
    Json *evaluated_args,
    struct function_data func_data
) {
    EvalData err = eval_from_json(json_invalid());

    Vec_ASTNode params = function_node->inner.function.args;
    assert(params.length == func_data.parameter_amount);

    for (int i = 0; i < params.length; i++) {
        evaluated_args[i] = json_null();
        ASTNode *node = params.data[i];
        // A closure can be any number less than or equal to JSON_TYPE_CLOSURE.
        //
        // This allows us to treat closures with multiple arguments as different types.
        // e.g. `|v|` is a different type than `|i, j, k|`
        if (func_data.parameter_types[i] <= JSON_TYPE_CLOSURE) {
            if (node->type != AST_TYPE_CLOSURE) {
                eval_set_err(e, EVAL_ERR_FUNC_NO_CLOSURE);
                goto cleanup;
            }
            assert(node->type == AST_TYPE_CLOSURE);

            int exp_closure_params = -((int)func_data.parameter_types[i] - JSON_TYPE_CLOSURE);
            uint act_closure_params = node->inner.closure.args.length;
            if (exp_closure_params != act_closure_params) {
                eval_set_err(
                    e, EVAL_ERR_FUNC_CLOSURE_PARAM_NUMBER(exp_closure_params, act_closure_params)
                );
                goto cleanup;
            }
        } else {
            if (node->type == AST_TYPE_CLOSURE) {
                eval_set_err(e, EVAL_ERR_FUNC_UNEXPECTED_CLOSURE);
                goto cleanup;
            }

            Json j = eval_to_json(e, eval_node(e, node));
            if (func_data.parameter_types[i] != JSON_TYPE_ANY) {
                EXPECT_TYPE(
                    e,
                    j,
                    func_data.parameter_types[i],
                    EVAL_ERR_FUNC_WRONG_ARGS(
                        json_type(func_data.parameter_types[i]), json_type(j.type)
                    )
                );
                goto cleanup;
            }
            evaluated_args[i] = j;
        }

        continue; // Don't cleanup
    cleanup:
        // We need to free all of what we've evaluated up to this point since we got an error
        //
        // Since i is the json we are adding this iteration, we want to include that, hence <=
        for (int j = 0; j <= i; j++) {
            json_free(evaluated_args[j]);
        }
        return;
    }
}

static EvalData func_expect_args(
    Eval *e,
    ASTNode *function_node,
    Json *evaluated_args,
    struct function_data func_data
) {
    EvalData err = eval_from_json(json_invalid());
    if (eval_has_err(e)) {
        return err;
    }
    assert(function_node->type == AST_TYPE_FUNCTION);

    // Make sure the amount of parameters match
    Vec_ASTNode args = function_node->inner.function.args;
    if (func_data.parameter_amount != args.length) {
        eval_set_err(e, EVAL_ERR_FUNC_PARAM_NUMBER(func_data.parameter_amount, args.length));
        return err;
    }

    // Make sure the caller is of the correct type
    EvalData caller = func_eval_caller(e, function_node, func_data);
    if (eval_has_err(e)) {
        return err;
    }

    // Make sure the parameters are of the correct type
    func_eval_params(e, function_node, evaluated_args, func_data);
    if (eval_has_err(e)) {
        free_eval_data(&caller);
        return err;
    }

    return caller;
}
