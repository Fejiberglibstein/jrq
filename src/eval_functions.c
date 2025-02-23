#include "eval_private.h"
#include "src/errors.h"
#include "src/eval.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <string.h>

static void vs_push_variables(VariableStack *vs, Vec_ASTNode closure) {
    for (int i = 0; i < closure.length; i++) {

        struct Variable var = (struct Variable) {
            .name = closure.data[i]->inner.primary.inner.ident,
            .value = json_null(),
        };

        vec_append(*vs, var);
    }
}

inline static EvalResult to_iter(EvalResult e) {
    if (e.type == EVAL_JSON) {
        if (e.json.type != JSON_TYPE_LIST) {
            // TODO better error messages
            return eval_res_error(TYPE_ERROR("i need a list here"));
        }
        return eval_res_iter(iter_list(e.json));
    }
    return e;
}

/// Will return a copy of the json value associated with the variable name
Json vs_get_variable(VariableStack *vs, char *var_name) {
    // Iterate through the stack in reverse order
    for (uint i = vs->length - 1; i >= 0; i--) {
        if (strcmp(vs->data[i].name, var_name) == 0) {
            return json_copy(vs->data[i].value);
        }
    }

    return json_invalid_msg(RUNTIME_ERROR("Variable not in scope: %s", var_name));
}

static Json map_func(Json in, void *captures) {
    ASTNode *node = (ASTNode *)captures;
    return json_null();
}

EvalResult eval_function_map(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);

    JsonIterator callee
        = EXPECT_ITER(to_iter(eval_node(e, node->inner.function.callee)), (Json[]) {});

    // TODO add error handling for parameters
    Vec_ASTNode args = node->inner.function.args;
    assert(args.data[0]->type == AST_TYPE_CLOSURE && args.length == 1);

    return eval_res_iter(iter_map(callee, &map_func, args.data[0]));
}

EvalResult eval_function_collect(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);

    JsonIterator callee = EXPECT_ITER(eval_node(e, node->inner.function.callee), (Json[]) {});
}
