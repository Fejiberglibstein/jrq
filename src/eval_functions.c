#include "eval_private.h"
#include "src/errors.h"
#include "src/json.h"
#include "src/parser.h"
#include <assert.h>
#include <string.h>

/// Will return a copy of the json value associated with the variable name
static Json get_variable(VariableStack *vs, char *var_name) {
    // Iterate through the stack in reverse order
    for (uint i = vs->length - 1; i >= 0; i--) {
        if (strcmp(vs->data[i].name, var_name) == 0) {
            return json_copy(vs->data[i].value);
        }
    }

    return json_invalid_msg(RUNTIME_ERROR("Variable not in scope: %s", var_name));
}

Json eval_function_map(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);

    Json callee = PROPOGATE_INVALID(eval_node(e, node->inner.function.callee), (Json[]) {});


}
