#ifndef _EVAL_H
#define _EVAL_H

#include "src/json_iter.h"
#include "src/parser.h"

typedef enum {
    EVAL_ERR,
    EVAL_ITER,
    EVAL_JSON,
} EvalResultType;

typedef struct {
    union {
        char *error;
        JsonIterator iter;
        Json json;
    };
    EvalResultType type;
} EvalResult;

EvalResult eval(ASTNode *node, Json input);

#endif // _EVAL_H
