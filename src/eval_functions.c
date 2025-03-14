#include "src/errors.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <stdint.h>
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

/// Push all the variables of a closure onto the stack.
///
/// Will return the amount of variables that were pushed onto the stack
int vs_push_closure_variable(Eval *e, ASTNode *var, Json value) {
    VariableStack *vs = &e->vs;
    switch (var->type) {
    case AST_TYPE_PRIMARY:
        assert(var->inner.primary.type == TOKEN_IDENT);
        vs_push_variable(vs, var->inner.primary.inner.ident, value);
        return 1;
        break;
    case AST_TYPE_LIST:
        if (value.type != JSON_TYPE_LIST) {
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        if (value.inner.list.length != var->inner.list.length) {
            // change the error message here, it's not accurate
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        int pushed = 0;
        for (int i = 0; i < value.inner.list.length; i++) {
            pushed += vs_push_closure_variable(e, var->inner.list.data[i], json_list_get(value, i));
            // set to null so we can free the list without worrying about freeing stuff inside the
            // list
            json_list_set(value, i, json_null());
        }
        json_free(value);
        return pushed;
        break;
    default:
        unreachable("Closure cannot be anything else");
    }
}

/// Pop all the variables of a closure onto the stack.
///
/// You should pass in the amount of variables that were pushed into this function through `amt`
///
/// Will return the amount popped off the stack
int vs_pop_closure_variable(Eval *e, ASTNode *var) {
    VariableStack *vs = &e->vs;
    int popped = 0;

    switch (var->type) {
    case AST_TYPE_PRIMARY:
        assert(var->inner.primary.type == TOKEN_IDENT);
        vs_pop_variable(vs, var->inner.primary.inner.ident);
        return 1;
    case AST_TYPE_LIST:
        for (int i = (int)var->inner.list.length - 1; i >= 0; i--) {
            popped += vs_pop_closure_variable(e, var->inner.list.data[i]);
        }
        return popped;
    default:
        unreachable("Closure cannot be anything else");
    }
}

//
// BEGIN FUNCTIONS
//

struct simple_closure {
    Eval *e;
    ASTNode *node;
    Vec_ASTNode params;
};

static Json mapper(Json j, void *aux) {
    struct simple_closure *c = aux;

    int pushed = vs_push_closure_variable(c->e, c->params.data[0], j);
    Json ret = eval_to_json(c->e, eval_node(c->e, c->node));
    int popped = vs_pop_closure_variable(c->e, c->params.data[0]);
    assert(pushed == popped);

    return ret;
}

static struct function_data FUNC_MAP = {
    .function_name = "map",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_CLOSURE_WITH_PARAMS(1)},
    .parameter_amount = 1,
};
static JsonIterator eval_func_map(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};

    Vec_ASTNode args = node->inner.function.args;
    EvalData i = func_expect_args(e, node, evaled_args, FUNC_MAP);
    if (eval_has_err(e)) {
        return NULL;
    }
    assert(args.data[0]->type == AST_TYPE_CLOSURE);

    // Safe to get iter here because we already made sure there were no errors
    JsonIterator iter = i.iter;

    struct simple_closure *c = malloc(sizeof(*c));

    *c = (struct simple_closure) {
        .e = e,
        .node = args.data[0]->inner.closure.body,
        .params = args.data[0]->inner.closure.args,
    };

    return iter_map(iter, &mapper, c, true);
}

static struct function_data FUNC_COLLECT = {
    .function_name = "collect",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static Json eval_func_collect(Eval *e, ASTNode *node) {
    EvalData j = func_expect_args(e, node, NULL, FUNC_COLLECT);
    if (eval_has_err(e)) {
        return json_invalid();
    }
    return eval_to_json(e, j);
}

static struct function_data FUNC_ITER = {
    .function_name = "iter",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static JsonIterator eval_func_iter(Eval *e, ASTNode *node) {
    EvalData i = func_expect_args(e, node, NULL, FUNC_ITER);
    if (eval_has_err(e)) {
        return NULL;
    }

    // func_expect_args will automatically cast into an iterator for us because we expect the type
    // to be an iterator
    return i.iter;
}

static struct function_data FUNC_KEYS = {
    .function_name = "keys",
    .caller_type = JSON_TYPE_OBJECT,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static JsonIterator eval_func_keys(Eval *e, ASTNode *node) {
    // Not expecting any arguments, hence the 0 length array
    Json evaled_args[0] = {};
    EvalData j = func_expect_args(e, node, evaled_args, FUNC_KEYS);
    if (eval_has_err(e)) {
        return NULL;
    }

    Json json = j.json;
    assert(json.type == JSON_TYPE_OBJECT);
    return iter_obj_keys(json);
}

static struct function_data FUNC_VALUES = {
    .function_name = "values",
    .caller_type = JSON_TYPE_OBJECT,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static JsonIterator eval_func_values(Eval *e, ASTNode *node) {
    // Not expecting any arguments, hence the 0 length array
    Json evaled_args[0] = {};

    EvalData j = func_expect_args(e, node, evaled_args, FUNC_VALUES);
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
