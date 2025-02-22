#include "eval_private.h"

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

