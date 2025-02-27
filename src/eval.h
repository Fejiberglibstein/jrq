#ifndef _EVAL_H
#define _EVAL_H

#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"

typedef enum {
    EVAL_ERR,
    EVAL_JSON,
    EVAL_ITER,
} EvalResultType;

typedef struct {
    union {
        char *error;
        Json json;
        JsonIterator iter;
    };
    EvalResultType type;
} EvalResult;

EvalResult eval(ASTNode *node, Json input);

#endif // _EVAL_H
