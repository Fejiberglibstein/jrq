#ifndef _EVAL_PRIVATE_H
#define _EVAL_PRIVATE_H
#include "eval.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"

#define _clean_up(FREE_LEN, FREE...)                                                               \
    for (int _i = 0; _i < FREE_LEN; _i++) {                                                        \
        json_free(FREE[_i]);                                                                       \
    }

#define unreachable(str) assert(false && str)

// will clean up everything in the free list and return from the function.
//
// This (i think) can be used at any point during the execution of the function,
// as long as it's before anything meaningful is done with the evaluated json.
// This way, we can delay bubbling until everything has been evaluated and we
// only have one bubble.
#define BUBBLE_ERROR(e, FREE...)                                                                   \
    if (e->err.err != NULL) {                                                                      \
        _clean_up(sizeof(FREE) / sizeof(*FREE), FREE);                                             \
        return eval_from_json(json_null());                                                        \
    }

#define EXPECT_TYPE(e, j, t, ERR...)                                                               \
    if (j.type != t && e->err.err == NULL) {                                                       \
        eval_set_err(e, ERR);                                                                      \
    }

#define eval_set_err(e, ERR...)                                                                    \
    if (e->err.err == NULL) {                                                                      \
        e->err = jrq_error(e->range, ERR);                                                         \
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

/// Create an EvalData from j
EvalData eval_from_json(Json j);
/// Create an EvalData from i
EvalData eval_from_iter(JsonIterator i);

#endif // _EVAL_PRIVATE_H
