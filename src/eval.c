#include "src/eval_private.h"

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
    if (d.json.type == JSON_TYPE_LIST) {
        return iter_list(d.json);
    }
    eval_set_err(e, "Expected Iterator, got %s", json_type(d.json.type));
    return NULL;
}
