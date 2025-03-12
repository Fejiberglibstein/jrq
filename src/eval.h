#ifndef _EVAL_H
#define _EVAL_H

#include "src/errors.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/lexer.h"
#include "src/parser.h"

typedef struct {
    union {
        JrqError err;
        Json json;
    };
    enum {
        EVAL_OK,
        EVAL_ERR,
    } type;
} EvalResult;

EvalResult eval(ASTNode *node, Json input);

#endif // _EVAL_H
