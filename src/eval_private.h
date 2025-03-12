#ifndef _EVAL_PRIVATE_H
#define _EVAL_PRIVATE_H
#include "eval.h"
#include "src/json_iter.h"
#include "src/parser.h"

#define _clean_up(FREE, FREE_LEN)                                                                  \
    for (int i = 0; i < FREE_LEN; i++) {                                                           \
        json_free(FREE[i]);                                                                        \
    }

#define BUBBLE_ERROR(E, FREE...)                                                                   \
    if (E->err.err != NULL) {                                                                      \
        _clean_up(FREE, sizeof(FREE) / sizeof(*FREE));                     \
        return eval_from_json(json_null());                                                        \
    }

typedef struct {
    union {
        Json json;
        JsonIterator iter;
    };
    enum {
        SOME_JSON,
        SOME_ITER,
    } type;
} EvalData;

typedef struct {
    /// The input json that the evaluator is called on
    Json input;

    /// Any error that has occurred while running. `err.err` will be NULL if
    /// there is no error, and not NULL if there is an error
    JrqError err;

    /// The range of the node that is currently being looked at.
    Range range;
} Eval;

/// If `d` is already json, do nothing.
/// Otherwise, implictly convert the iterator into json
Json eval_to_json(Eval *e, EvalData d);
/// If `d` is already an iterator, do nothing.
/// Otherwise, implictly convert d if it is a json list to an iterator.
///
/// If d is not a json list, this function will cause e->err to be set.
JsonIterator eval_to_iter(Eval *e, EvalData d);

EvalData eval_node(Eval *e, ASTNode *node);

#endif // _EVAL_PRIVATE_H
