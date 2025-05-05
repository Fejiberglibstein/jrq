#include "src/errors.h"
#include "src/eval/private.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/json_serde.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Don't use this with a T of any, instead just use the normal JSON_TYPE_LIST value
#define JSON_TYPE_LIST_T(v) (JSON_TYPE_LIST + v)
#define JSON_TYPE_ITERATOR (-1)
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
/// - JSON_TYPE_LIST_T                   (list that must be a specific type)
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
void vs_push_variable(VariableStack *vs, char *var_name, Json value);
void vs_pop_variable(VariableStack *vs, char *var_name);
Json vs_get_variable(Eval *e, char *var_name);
int vs_push_closure_variable(Eval *e, ASTNode *var, Json value);
int vs_pop_closure_variable(Eval *e, ASTNode *var, Json value);

struct simple_closure {
    Eval *e;
    ASTNode *node;
    Vec_ASTNode params;
};

static Json closure_returns_json(Json j, void *aux) {
    struct simple_closure *c = aux;

    int pushed = vs_push_closure_variable(c->e, c->params.data[0], j);
    Json ret = json_invalid();
    if (!eval_has_err(c->e)) {
        ret = eval_to_json(c->e, eval_node(c->e, c->node));
    }
    int popped = vs_pop_closure_variable(c->e, c->params.data[0], j);
    assert(pushed == popped);
    json_free(j);

    return ret;
}
static bool closure_returns_bool(Json j, void *aux) {
    struct simple_closure *c = aux;

    int pushed = vs_push_closure_variable(c->e, c->params.data[0], j);
    Json ret = json_invalid();
    if (!eval_has_err(c->e)) {
        ret = eval_to_json(c->e, eval_node(c->e, c->node));
    }
    int popped = vs_pop_closure_variable(c->e, c->params.data[0], j);
    assert(pushed == popped);
    // We should not free j here like we do for mapping, because the json should
    // still exist after the filter is done.

    EXPECT_TYPE(
        c->e,
        ret.type,
        JSON_TYPE_BOOL,
        EVAL_ERR_CLOSURE_RETURN(JSON_TYPE(JSON_TYPE_BOOL), json_type(ret))
    );
    if (eval_has_err(c->e)) {
        return false;
    }

    return json_get_bool(ret);
}

// Helper function to eliminate much of the boilerplate in making eval functions
// from iterators
//
// Returns true if everything was sucessful, and false if there was an error.
bool iter_eval_func(
    Eval *e,
    ASTNode *node,
    struct function_data function,
    Json *evaled_args,
    JsonIterator *iter,
    struct simple_closure **closure
) {
    Vec_ASTNode args = node->inner.function.args;
    EvalData i = func_expect_args(e, node, evaled_args, function);
    if (eval_has_err(e)) {
        return false;
    }
    assert(args.data[0]->type == AST_TYPE_CLOSURE);

    // Safe to get iter here because we already made sure there were no errors
    *iter = i.iter;

    struct simple_closure *c = malloc(sizeof(*c));

    *c = (struct simple_closure) {
        .e = e,
        .node = args.data[0]->inner.closure.body,
        .params = args.data[0]->inner.closure.args,
    };

    *closure = c;

    return true;
}

static struct function_data FUNC_MAP = {
    .function_name = "map",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_CLOSURE_WITH_PARAMS(1)},
    .parameter_amount = 1,
};
static JsonIterator eval_func_map(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};
    JsonIterator iter;
    struct simple_closure *c;

    if (!iter_eval_func(e, node, FUNC_MAP, evaled_args, &iter, &c)) {
        return NULL;
    }
    return iter_map(iter, &closure_returns_json, c, true);
}

static struct function_data FUNC_FILTER = {
    .function_name = "filter",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_CLOSURE_WITH_PARAMS(1)},
    .parameter_amount = 1,
};
static JsonIterator eval_func_filter(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};
    JsonIterator iter;
    struct simple_closure *c;

    if (!iter_eval_func(e, node, FUNC_FILTER, evaled_args, &iter, &c)) {
        return NULL;
    }
    return iter_filter(iter, &closure_returns_bool, c, true);
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

static struct function_data FUNC_ENUMERATE = {
    .function_name = "enumerate",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static JsonIterator eval_func_enumerate(Eval *e, ASTNode *node) {
    // Not expecting any arguments, hence the 0 length array
    Json evaled_args[0] = {};

    EvalData j = func_expect_args(e, node, evaled_args, FUNC_ENUMERATE);
    if (eval_has_err(e)) {
        return NULL;
    }

    JsonIterator iter = j.iter;
    return iter_enumerate(iter);
}

static struct function_data FUNC_ZIP = {
    .function_name = "zip",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_LIST},
    .parameter_amount = 1,
};
static JsonIterator eval_func_zip(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {};

    EvalData j = func_expect_args(e, node, evaled_args, FUNC_ZIP);
    if (eval_has_err(e)) {
        return NULL;
    }
    assert(evaled_args[0].type == JSON_TYPE_LIST);

    JsonIterator a = j.iter;
    // TODO maybe expect_args can expect an iterator type here instead of a
    // list type
    JsonIterator b = iter_list(evaled_args[0]);

    return iter_zip(a, b);
}

static struct function_data FUNC_SUM = {
    .function_name = "sum",
    .caller_type = JSON_TYPE_LIST_T(JSON_TYPE_NUMBER),

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static Json eval_func_sum(Eval *e, ASTNode *node) {
    Json evaled_args[0] = {};

    EvalData d = func_expect_args(e, node, evaled_args, FUNC_SUM);
    if (eval_has_err(e)) {
        return json_invalid();
    }

    Json j = d.json;

    double sum = 0;
    for (size_t i = 0; i < json_list_length(j); i++) {
        Json el = json_list_get(j, i);
        sum += json_get_number(el);
        json_free(el);
    }

    return json_number(sum);
}

static struct function_data FUNC_PRODUCT = {
    .function_name = "product",
    .caller_type = JSON_TYPE_LIST_T(JSON_TYPE_NUMBER),

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static Json eval_func_product(Eval *e, ASTNode *node) {
    Json evaled_args[0] = {};

    EvalData d = func_expect_args(e, node, evaled_args, FUNC_PRODUCT);
    if (eval_has_err(e)) {
        return json_invalid();
    }

    Json j = d.json;

    double product = 1;
    for (size_t i = 0; i < json_list_length(j); i++) {
        Json el = json_list_get(j, i);
        product *= json_get_number(el);
        json_free(el);
    }

    return json_number(product);
}

static struct function_data FUNC_FLATTEN = {
    .function_name = "flatten",
    .caller_type = JSON_TYPE_LIST,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static Json eval_func_flatten(Eval *e, ASTNode *node) {
    Json evaled_args[0] = {};

    EvalData d = func_expect_args(e, node, evaled_args, FUNC_FLATTEN);
    if (eval_has_err(e)) {
        return json_invalid();
    }
    assert(d.type == SOME_JSON);
    Json json = d.json;
    assert(json.type == JSON_TYPE_LIST);

    switch (json.list_inner_type) {
    case JSON_TYPE_LIST:
        Json list = json_list();
        for (size_t i = 0; i < json_list_length(json); i++) {
            Json el = json_list_get(json, i);
            for (size_t j = 0; j < json_list_length(el); j++) {
                list = json_list_append(list, json_copy(json_list_get(el, j)));
            }
        }
        json_free(json);
        return list;
        break;
    case JSON_TYPE_OBJECT:
        Json object = json_object();
        for (size_t i = 0; i < json_list_length(json); i++) {
            Json el = json_copy(json_list_get(json, i));
            JsonIterator keyset = iter_obj_key_value(el);

            IterOption _pair;
            for (_pair = iter_next(keyset); _pair.type != ITER_DONE; _pair = iter_next(keyset)) {
                Json pair = _pair.some;
                json_object_set(
                    object, json_copy(json_list_get(pair, 0)), json_copy(json_list_get(pair, 1))
                );
                json_free(pair);
            }
            iter_free(keyset);
        }
        json_free(json);
        return object;
        break;
    default:
        EXPECT_TYPE(
            e,
            json.list_inner_type,
            -1,
            EVAL_ERR_FUNC_WRONG_CALLER("object or list", JSON_TYPE(json.list_inner_type))
        );
        json_free(json);
        return json_null();
        break;
    }
}

static struct function_data FUNC_JOIN = {
    .function_name = "join",
    .caller_type = JSON_TYPE_LIST_T(JSON_TYPE_STRING),

    .parameter_types = (JsonType[]) {JSON_TYPE_STRING},
    .parameter_amount = 1,
};
static Json eval_func_join(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {};

    EvalData d = func_expect_args(e, node, evaled_args, FUNC_JOIN);
    if (eval_has_err(e)) {
        return json_invalid();
    }
    assert(d.type == SOME_JSON);
    Json str_list = d.json;
    assert(str_list.type == JSON_TYPE_LIST);
    assert(evaled_args[0].type == JSON_TYPE_STRING);

    Json string = json_string("");

    for (size_t i = 0; i < json_list_length(str_list); i++) {
        if (i != 0) {
            json_string_concat(string, evaled_args[0]);
        }
        json_string_concat(string, json_list_get(str_list, i));
    }

    json_free(evaled_args[0]);
    json_free(str_list);

    return string;
}

static struct function_data FUNC_LENGTH = {
    .function_name = "length",
    .caller_type = JSON_TYPE_ANY,

    .parameter_types = (JsonType[]) {},
    .parameter_amount = 0,
};
static Json eval_func_length(Eval *e, ASTNode *node) {
    Json evaled_args[0] = {};

    EvalData d = func_expect_args(e, node, evaled_args, FUNC_LENGTH);
    if (eval_has_err(e)) {
        return json_invalid();
    }
    assert(d.type == SOME_JSON);
    Json j = d.json;

    size_t length;
    switch (j.type) {
    case JSON_TYPE_LIST:
        length = json_list_length(j);
        json_free(j);
        break;
    case JSON_TYPE_STRING:
        length = json_string_length(j);
        json_free(j);
        break;
    default:
        EXPECT_TYPE(
            e, j.list_inner_type, -1, EVAL_ERR_FUNC_WRONG_CALLER("string or list", json_type(j))
        );
        json_free(j);
        return json_null();
        break;
    }

    return json_number((float)length);
}

// TODO:
//
// skip_while, take_while, and filter are all identical functions, besides the last line when they
// return an iterator. Maybe make some macro or smth that does everything for you, so there is less
// code.
//
// Maybe not a macro because that will be hard to read, but at least some form of function

static struct function_data FUNC_SKIP_WHILE = {
    .function_name = "skip_while",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_CLOSURE_WITH_PARAMS(1)},
    .parameter_amount = 1,
};
static JsonIterator eval_func_skip_while(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};
    JsonIterator iter;
    struct simple_closure *c;

    if (!iter_eval_func(e, node, FUNC_SKIP_WHILE, evaled_args, &iter, &c)) {
        return NULL;
    }
    return iter_skip_while(iter, &closure_returns_bool, c, true);
}

static struct function_data FUNC_TAKE_WHILE = {
    .function_name = "take_while",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_CLOSURE_WITH_PARAMS(1)},
    .parameter_amount = 1,
};
static JsonIterator eval_func_take_while(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};
    JsonIterator iter;
    struct simple_closure *c;

    if (!iter_eval_func(e, node, FUNC_TAKE_WHILE, evaled_args, &iter, &c)) {
        return NULL;
    }
    return iter_take_while(iter, &closure_returns_bool, c, true);
}

EvalData eval_node_function(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    char *func_name = node->inner.function.function_name.inner.string;
    e->range = node->range;

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
    } else if (strcmp(func_name, "enumerate") == 0) {
        return eval_from_iter(eval_func_enumerate(e, node));
    } else if (strcmp(func_name, "filter") == 0) {
        return eval_from_iter(eval_func_filter(e, node));
    } else if (strcmp(func_name, "zip") == 0) {
        return eval_from_iter(eval_func_zip(e, node));
    } else if (strcmp(func_name, "sum") == 0) {
        return eval_from_json(eval_func_sum(e, node));
    } else if (strcmp(func_name, "product") == 0) {
        return eval_from_json(eval_func_product(e, node));
    } else if (strcmp(func_name, "flatten") == 0) {
        return eval_from_json(eval_func_flatten(e, node));
    } else if (strcmp(func_name, "join") == 0) {
        return eval_from_json(eval_func_join(e, node));
    } else if (strcmp(func_name, "length") == 0) {
        return eval_from_json(eval_func_length(e, node));
    } else if (strcmp(func_name, "skip_while") == 0) {
        return eval_from_iter(eval_func_skip_while(e, node));
    } else if (strcmp(func_name, "take_while") == 0) {
        return eval_from_iter(eval_func_take_while(e, node));
    }

    eval_set_err(e, EVAL_ERR_FUNC_NOT_FOUND(func_name));
    BUBBLE_ERROR(e, (Json[]) {});
    unreachable("Bubbling would have returned the error");
}

void vs_push_variable(VariableStack *vs, char *var_name, Json value) {
    vec_append(*vs, (Variable) {.value = value, .name = var_name});
}

void vs_pop_variable(VariableStack *vs, char *var_name) {
    Variable v = vec_pop(*vs);
    assert(strcmp(v.name, var_name) == 0);
}

Json vs_get_variable(Eval *e, char *var_name) {
    VariableStack *vs = &e->vs;
    for (uint i = vs->length - 1; i >= 0; i--) {
        if (strcmp(vs->data[i].name, var_name) == 0) {
            return json_copy(vs->data[i].value);
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
        if (json_get_list(value)->length != var->inner.list.length) {
            // change the error message here, it's not accurate
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        int pushed = 0;
        for (int i = 0; i < json_get_list(value)->length; i++) {
            pushed += vs_push_closure_variable(e, var->inner.list.data[i], json_list_get(value, i));
        }
        return pushed;
        break;
    default:
        unreachable("Closure cannot be anything else");
    }
}

/// Pop all the variables of a closure onto the stack.
///
/// Will return the amount popped off the stack
int vs_pop_closure_variable(Eval *e, ASTNode *var, Json value) {
    VariableStack *vs = &e->vs;
    int popped = 0;

    switch (var->type) {
    case AST_TYPE_PRIMARY:
        assert(var->inner.primary.type == TOKEN_IDENT);
        vs_pop_variable(vs, var->inner.primary.inner.ident);
        return 1;
    case AST_TYPE_LIST:
        if (value.type != JSON_TYPE_LIST) {
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        if (json_get_list(value)->length != var->inner.list.length) {
            // change the error message here, it's not accurate
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        for (int i = (int)var->inner.list.length - 1; i >= 0; i--) {
            popped += vs_pop_closure_variable(e, var->inner.list.data[i], json_list_get(value, i));
        }
        return popped;
    default:
        unreachable("Closure cannot be anything else");
    }
}

/// Evaluate and error check the caller of a function.
///
/// In `"foo".bar()`, `"foo"` is the caller.
///
/// This should not really be used ever, instead use `func_expect_args`
static EvalData func_eval_caller(Eval *e, ASTNode *function_node, struct function_data func) {
    EvalData err = eval_from_json(json_invalid());

    EvalData caller = eval_node(e, function_node->inner.function.callee);

    if (func.caller_type != JSON_TYPE_ITERATOR && func.caller_type != JSON_TYPE_ANY) {
        // cast caller into json
        Json jcaller = eval_to_json(e, caller);
        if (eval_has_err(e)) {
            json_free(jcaller);
            return err;
        }

        if (func.caller_type > JSON_TYPE_LIST) {
            int list_inner_type = func.caller_type - JSON_TYPE_LIST;
            EXPECT_TYPE(
                e,
                jcaller.list_inner_type,
                list_inner_type,
                EVAL_ERR_FUNC_WRONG_CALLER(
                    json_type((Json) {.type = JSON_TYPE_LIST, .list_inner_type = list_inner_type}),
                    json_type(jcaller)
                )
            );
            if (eval_has_err(e)) {
                json_free(jcaller);
                return err;
            }
        } else {
            EXPECT_TYPE(
                e,
                jcaller.type,
                func.caller_type,
                EVAL_ERR_FUNC_WRONG_CALLER(JSON_TYPE(func.caller_type), json_type(jcaller))
            );
            if (eval_has_err(e)) {
                json_free(jcaller);
                return err;
            }
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

/// Evaluate and error check the parameters of a function call.
///
/// This should not really be used ever, instead use `func_expect_args`
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
                    j.type,
                    func_data.parameter_types[i],
                    EVAL_ERR_FUNC_WRONG_ARGS(JSON_TYPE(func_data.parameter_types[i]), json_type(j))
                );
                if (eval_has_err(e)) {
                    goto cleanup;
                }
            }
            evaluated_args[i] = j;
        }

        continue; // Don't cleanup
    cleanup:
        // We need to free all of what we've evaluated up to this point since we got an error
        //
        // Since i is the json we are adding this iteration, we want to include that, hence <=
        for (int j = 0; j < i; j++) {
            json_free(evaluated_args[j]);
        }
        return;
    }
}

/// Ensure that a function call matches the expected arguments
///
/// Will return an err if any part of the function call was wrong, e.g. wrong parameter types,
/// wrong callee type, or wrong number of parameters.
///
/// If there was no error, the function will return the evaluated caller:
/// {"foo": 10}.map( ... ) will return {"foo": 10}
///
/// `evaluated_args` should be passed in as an empty list of size `func_data.parameter_amount`.
/// This method will fill the list with the evaluated parameter types for the function call. In
/// `foo(10 - 4, 2)`, evaluated args will be become the list [6, 2].
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
