#include "src/eval.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include <assert.h>
#include <stdio.h>

// clang-format off
EvalData eval_from_json(Json j) { return (EvalData) {.type = SOME_JSON, .json = j}; }
EvalData eval_from_iter(JsonIterator i) { return (EvalData) {.type = SOME_ITER, .iter = i}; }
// clang-format on

Json eval_to_json(Eval *e, EvalData d) {
    if (d.type == SOME_JSON) {
        return d.json;
    }
    return iter_collect(d.iter);
}

JsonIterator eval_to_iter(Eval *e, EvalData d) {
    if (d.type == SOME_ITER) {
        return d.iter;
    }
    switch (d.json.type) {
    case JSON_TYPE_LIST:
        return iter_list(d.json);
    case JSON_TYPE_OBJECT:
        return iter_obj_key_value(d.json);
    default:
        eval_set_err(e, "Expected Iterator, got %s", json_type(d.json.type));
        return NULL;
    }
}

EvalResult eval(ASTNode *node, Json input) {
    Eval e = (Eval) {
        .input = input,
        .err = {0},
    };

    EvalData j = eval_node(&e, node);
    Json result = eval_to_json(&e, j);
    ast_free(node);

    assert(e.vs.length == 0);
    if (e.vs.data != NULL) {
        free(e.vs.data);
    }

    if (e.err.err != NULL) {
        json_free(result);
        return (EvalResult) {.err = e.err, .type = RES_ERR};
    } else {
        return (EvalResult) {.json = result, .type = RES_OK};
    }
}
