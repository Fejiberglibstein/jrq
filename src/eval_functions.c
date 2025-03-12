#include "src/eval_private.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include <assert.h>
#include <string.h>

static JsonIterator eval_func_map(Eval *e, ASTNode *node) {
}

EvalData eval_node_function(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    char *func_name = node->inner.function.function_name.inner.string;
    if (strcmp(func_name, "map") == 0) {
        return eval_from_iter(eval_func_map(e, node));
    } else {
        eval_set_err(e, EVAL_ERR_FUNC_NOT_FOUND(func_name));
        BUBBLE_ERROR(e, (Json[]) {});
        unreachable("Bubbling would have returned the error");
    }
}
