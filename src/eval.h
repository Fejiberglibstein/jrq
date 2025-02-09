#ifndef _EVAL_H
#define _EVAL_H

#include "src/json_serde.h"
#include "src/parser.h"

Json *eval(ASTNode *node, Json *input);

#endif // _EVAL_H
