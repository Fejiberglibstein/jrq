#include "src/eval.h"
#include "src/errors.h"
#include "src/eval_private.h"
#include "src/json.h"
#include "src/json_iter.h"

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

    e->err = (JrqError) {
        .err = TYPE_ERROR("Expected iterator, got %s", json_type(&d.json)),
    }
}

EvalResult eval(ASTNode *node, Json input) {
    Eval e = (Eval) {
        .input = input,
        .err = {0},
    };

    EvalData j = eval_node(&e, node);
    if (e.err.err != NULL) {
        return (EvalResult) {
            .err = e.err,
            .type = EVAL_ERR,
        };
    } else {
        return (EvalResult) {
            .json = 
        }
    }
}

EvalData eval_node(Eval *e, ASTNode *node) {
}
