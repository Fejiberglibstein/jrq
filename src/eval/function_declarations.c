#include "src/errors.h"
#include "src/eval/private.h"
#include "src/eval/functions.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/utils.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
JsonIterator eval_func_map(Eval *e, ASTNode *node) {
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
JsonIterator eval_func_filter(Eval *e, ASTNode *node) {
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
Json eval_func_collect(Eval *e, ASTNode *node) {
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
JsonIterator eval_func_iter(Eval *e, ASTNode *node) {
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
JsonIterator eval_func_keys(Eval *e, ASTNode *node) {
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
JsonIterator eval_func_values(Eval *e, ASTNode *node) {
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
JsonIterator eval_func_enumerate(Eval *e, ASTNode *node) {
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
JsonIterator eval_func_zip(Eval *e, ASTNode *node) {
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
Json eval_func_sum(Eval *e, ASTNode *node) {
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
Json eval_func_product(Eval *e, ASTNode *node) {
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
Json eval_func_flatten(Eval *e, ASTNode *node) {
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
Json eval_func_join(Eval *e, ASTNode *node) {
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
Json eval_func_length(Eval *e, ASTNode *node) {
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

static struct function_data FUNC_SKIP_WHILE = {
    .function_name = "skip_while",
    .caller_type = JSON_TYPE_ITERATOR,

    .parameter_types = (JsonType[]) {JSON_TYPE_CLOSURE_WITH_PARAMS(1)},
    .parameter_amount = 1,
};
JsonIterator eval_func_skip_while(Eval *e, ASTNode *node) {
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
JsonIterator eval_func_take_while(Eval *e, ASTNode *node) {
    Json evaled_args[1] = {0};
    JsonIterator iter;
    struct simple_closure *c;

    if (!iter_eval_func(e, node, FUNC_TAKE_WHILE, evaled_args, &iter, &c)) {
        return NULL;
    }
    return iter_take_while(iter, &closure_returns_bool, c, true);
}

