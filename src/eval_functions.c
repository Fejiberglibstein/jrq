#include "eval_private.h"
#include "src/errors.h"
#include "src/eval.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"
#include "src/vector.h"
#include <assert.h>
#include <string.h>

#define EXPECT_ARGS(args, number, function_name, range)                                            \
    if (args.length != number) {                                                                   \
        return eval_res_error(TYPE_ERROR(                                                          \
            range,                                                                                 \
            "Invalid number of parameters to " function_name " (Expected %d, got %d)",             \
            number,                                                                                \
            args.length                                                                            \
        ));                                                                                        \
    }

static void vs_push_variables(VariableStack *vs, Vec_ASTNode closure) {
    for (int i = 0; i < closure.length; i++) {

        struct Variable var = (struct Variable) {
            .name = closure.data[i]->inner.primary.inner.ident,
            .value = json_null(),
        };

        vec_append(*vs, var);
    }
}

inline static EvalResult to_iter(EvalResult e, Range r) {
    if (e.type == EVAL_JSON) {
        if (e.json.type != JSON_TYPE_LIST) {
            return eval_res_error(
                TYPE_ERROR(r, "Could not convert %s into an Iterator", json_type(e.json.type))
            );
        }
        return eval_res_iter(iter_list(e.json));
    }
    return e;
}

/// Will return a copy of the json value associated with the variable name
EvalResult vs_get_variable(VariableStack *vs, char *var_name, Range r) {
    // Iterate through the stack in reverse order
    for (uint i = vs->length - 1; i >= 0; i--) {
        if (strcmp(vs->data[i].name, var_name) == 0) {
            return eval_res_json(json_copy(vs->data[i].value));
        }
    }

    return eval_res_error(RUNTIME_ERROR(r, "Variable not in scope: %s", var_name));
}

static Json map_func(Json in, void *captures) {
    ASTNode *node = (ASTNode *)captures;
    return json_null();
}

EvalResult eval_function_map(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    assert(node->inner.function.args.data[0]->type == AST_TYPE_CLOSURE);

    JsonIterator callee = EXPECT_ITER(
        node->range, to_iter(eval_node(e, node->inner.function.callee), node->range), (Json[]) {}
    );

    Vec_ASTNode args = node->inner.function.args;
    EXPECT_ARGS(args, 1, "map", node->range)

    return eval_res_iter(iter_map(callee, &map_func, args.data[0]));
}

EvalResult eval_function_collect(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    assert(node->inner.function.args.data[0]->type == AST_TYPE_CLOSURE);

    JsonIterator callee
        = EXPECT_ITER(node->range, eval_node(e, node->inner.function.callee), (Json[]) {});

    Vec_ASTNode args = node->inner.function.args;
    EXPECT_ARGS(args, 0, "collect", node->range)

    return eval_res_json(iter_collect(callee));
}
