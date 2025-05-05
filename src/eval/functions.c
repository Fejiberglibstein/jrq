#include "src/eval/functions.h"
#include "src/eval/function_declarations.h"
#include "src/utils.h"
#include <assert.h>
#include <string.h>

EvalData eval_node_function(Eval *e, ASTNode *node) {
    assert(node->type == AST_TYPE_FUNCTION);
    char *func_name = node->inner.function.function_name.inner.string;
    e->range = node->range;

    if (strcmp(func_name, "map") == 0) {
        return eval_from_iter(eval_func_map(e, node));
    } else if (strcmp(func_name, "collect") == 0) {
        return eval_from_json(eval_func_collect(e, node));
    } else if (strcmp(func_name, "iter") == 0) {
        return eval_from_iter(eval_func_iter(e, node));
    } else if (strcmp(func_name, "keys") == 0) {
        return eval_from_iter(eval_func_keys(e, node));
    } else if (strcmp(func_name, "values") == 0) {
        return eval_from_iter(eval_func_values(e, node));
    } else if (strcmp(func_name, "enumerate") == 0) {
        return eval_from_iter(eval_func_enumerate(e, node));
    } else if (strcmp(func_name, "filter") == 0) {
        return eval_from_iter(eval_func_filter(e, node));
    } else if (strcmp(func_name, "zip") == 0) {
        return eval_from_iter(eval_func_zip(e, node));
    } else if (strcmp(func_name, "sum") == 0) {
        return eval_from_json(eval_func_sum(e, node));
    } else if (strcmp(func_name, "product") == 0) {
        return eval_from_json(eval_func_product(e, node));
    } else if (strcmp(func_name, "flatten") == 0) {
        return eval_from_json(eval_func_flatten(e, node));
    } else if (strcmp(func_name, "join") == 0) {
        return eval_from_json(eval_func_join(e, node));
    } else if (strcmp(func_name, "length") == 0) {
        return eval_from_json(eval_func_length(e, node));
    } else if (strcmp(func_name, "skip_while") == 0) {
        return eval_from_iter(eval_func_skip_while(e, node));
    } else if (strcmp(func_name, "take_while") == 0) {
        return eval_from_iter(eval_func_take_while(e, node));
    }

    eval_set_err(e, EVAL_ERR_FUNC_NOT_FOUND(func_name));
    BUBBLE_ERROR(e, (Json[]) {});
    unreachable("Bubbling would have returned the error");
}

void vs_push_variable(VariableStack *vs, char *var_name, Json value) {
    vec_append(*vs, (Variable) {.value = value, .name = var_name});
}

void vs_pop_variable(VariableStack *vs, char *var_name) {
    Variable v = vec_pop(*vs);
    assert(strcmp(v.name, var_name) == 0);
}

Json vs_get_variable(Eval *e, char *var_name) {
    VariableStack *vs = &e->vs;
    for (uint i = vs->length - 1; i >= 0; i--) {
        if (strcmp(vs->data[i].name, var_name) == 0) {
            return json_copy(vs->data[i].value);
        }
    }

    eval_set_err(e, EVAL_ERR_VAR_NOT_FOUND(var_name));
    return json_null();
}

/// Push all the variables of a closure onto the stack.
///
/// Will return the amount of variables that were pushed onto the stack
int vs_push_closure_variable(Eval *e, ASTNode *var, Json value) {
    VariableStack *vs = &e->vs;
    switch (var->type) {
    case AST_TYPE_PRIMARY:
        assert(var->inner.primary.type == TOKEN_IDENT);
        vs_push_variable(vs, var->inner.primary.inner.ident, value);
        return 1;
        break;
    case AST_TYPE_LIST:
        if (value.type != JSON_TYPE_LIST) {
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        if (json_get_list(value)->length != var->inner.list.length) {
            // change the error message here, it's not accurate
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        int pushed = 0;
        for (int i = 0; i < json_get_list(value)->length; i++) {
            pushed += vs_push_closure_variable(e, var->inner.list.data[i], json_list_get(value, i));
        }
        return pushed;
        break;
    default:
        unreachable("Closure cannot be anything else");
    }
}

/// Pop all the variables of a closure onto the stack.
///
/// Will return the amount popped off the stack
int vs_pop_closure_variable(Eval *e, ASTNode *var, Json value) {
    VariableStack *vs = &e->vs;
    int popped = 0;

    switch (var->type) {
    case AST_TYPE_PRIMARY:
        assert(var->inner.primary.type == TOKEN_IDENT);
        vs_pop_variable(vs, var->inner.primary.inner.ident);
        return 1;
    case AST_TYPE_LIST:
        if (value.type != JSON_TYPE_LIST) {
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        if (json_get_list(value)->length != var->inner.list.length) {
            // change the error message here, it's not accurate
            eval_set_err(e, EVAL_ERR_FUNC_CLOSURE_TUPLE);
            return 0;
        }
        for (int i = (int)var->inner.list.length - 1; i >= 0; i--) {
            popped += vs_pop_closure_variable(e, var->inner.list.data[i], json_list_get(value, i));
        }
        return popped;
    default:
        unreachable("Closure cannot be anything else");
    }
}

/// Evaluate and error check the caller of a function.
///
/// In `"foo".bar()`, `"foo"` is the caller.
///
/// This should not really be used ever, instead use `func_expect_args`
static EvalData func_eval_caller(Eval *e, ASTNode *function_node, struct function_data func) {
    EvalData err = eval_from_json(json_invalid());

    EvalData caller = eval_node(e, function_node->inner.function.callee);

    if (func.caller_type != JSON_TYPE_ITERATOR && func.caller_type != JSON_TYPE_ANY) {
        // cast caller into json
        Json jcaller = eval_to_json(e, caller);
        if (eval_has_err(e)) {
            json_free(jcaller);
            return err;
        }

        if (func.caller_type > JSON_TYPE_LIST) {
            int list_inner_type = func.caller_type - JSON_TYPE_LIST;
            EXPECT_TYPE(
                e,
                jcaller.list_inner_type,
                list_inner_type,
                EVAL_ERR_FUNC_WRONG_CALLER(
                    json_type((Json) {.type = JSON_TYPE_LIST, .list_inner_type = list_inner_type}),
                    json_type(jcaller)
                )
            );
            if (eval_has_err(e)) {
                json_free(jcaller);
                return err;
            }
        } else {
            EXPECT_TYPE(
                e,
                jcaller.type,
                func.caller_type,
                EVAL_ERR_FUNC_WRONG_CALLER(JSON_TYPE(func.caller_type), json_type(jcaller))
            );
            if (eval_has_err(e)) {
                json_free(jcaller);
                return err;
            }
        }

        // Cast back into an EvalData like this because when we casted caller into json we may have
        // converted an iterator into a list.
        caller = eval_from_json(jcaller);
    } else if (func.caller_type == JSON_TYPE_ITERATOR) {
        JsonIterator icaller = eval_to_iter(e, caller);
        if (eval_has_err(e)) {
            if (icaller != NULL) {
                iter_free(icaller);
            }
            return err;
        }

        // Cast back into an EvalData for the same reason as above.
        caller = eval_from_iter(icaller);
    }
    return caller;
}

/// Evaluate and error check the parameters of a function call.
///
/// This should not really be used ever, instead use `func_expect_args`
static void func_eval_params(
    Eval *e,
    ASTNode *function_node,
    Json *evaluated_args,
    struct function_data func_data
) {
    Vec_ASTNode params = function_node->inner.function.args;
    assert(params.length == func_data.parameter_amount);

    for (int i = 0; i < params.length; i++) {
        evaluated_args[i] = json_null();
        ASTNode *node = params.data[i];
        // A closure can be any number less than or equal to JSON_TYPE_CLOSURE.
        //
        // This allows us to treat closures with multiple arguments as different types.
        // e.g. `|v|` is a different type than `|i, j, k|`
        if (func_data.parameter_types[i] <= JSON_TYPE_CLOSURE) {
            if (node->type != AST_TYPE_CLOSURE) {
                eval_set_err(e, EVAL_ERR_FUNC_NO_CLOSURE);
                goto cleanup;
            }
            assert(node->type == AST_TYPE_CLOSURE);

            int exp_closure_params = -((int)func_data.parameter_types[i] - JSON_TYPE_CLOSURE);
            uint act_closure_params = node->inner.closure.args.length;
            if (exp_closure_params != act_closure_params) {
                eval_set_err(
                    e, EVAL_ERR_FUNC_CLOSURE_PARAM_NUMBER(exp_closure_params, act_closure_params)
                );
                goto cleanup;
            }
        } else {
            if (node->type == AST_TYPE_CLOSURE) {
                eval_set_err(e, EVAL_ERR_FUNC_UNEXPECTED_CLOSURE);
                goto cleanup;
            }

            Json j = eval_to_json(e, eval_node(e, node));
            if (func_data.parameter_types[i] != JSON_TYPE_ANY) {
                EXPECT_TYPE(
                    e,
                    j.type,
                    func_data.parameter_types[i],
                    EVAL_ERR_FUNC_WRONG_ARGS(JSON_TYPE(func_data.parameter_types[i]), json_type(j))
                );
                if (eval_has_err(e)) {
                    goto cleanup;
                }
            }
            evaluated_args[i] = j;
        }

        continue; // Don't cleanup
    cleanup:
        // We need to free all of what we've evaluated up to this point since we got an error
        //
        // Since i is the json we are adding this iteration, we want to include that, hence <=
        for (int j = 0; j < i; j++) {
            json_free(evaluated_args[j]);
        }
        return;
    }
}

/// Ensure that a function call matches the expected arguments
///
/// Will return an err if any part of the function call was wrong, e.g. wrong parameter types,
/// wrong callee type, or wrong number of parameters.
///
/// If there was no error, the function will return the evaluated caller:
/// {"foo": 10}.map( ... ) will return {"foo": 10}
///
/// `evaluated_args` should be passed in as an empty list of size `func_data.parameter_amount`.
/// This method will fill the list with the evaluated parameter types for the function call. In
/// `foo(10 - 4, 2)`, evaluated args will be become the list [6, 2].
EvalData func_expect_args(
    Eval *e,
    ASTNode *function_node,
    Json *evaluated_args,
    struct function_data func_data
) {
    EvalData err = eval_from_json(json_invalid());
    if (eval_has_err(e)) {
        return err;
    }
    assert(function_node->type == AST_TYPE_FUNCTION);

    // Make sure the amount of parameters match
    Vec_ASTNode args = function_node->inner.function.args;
    if (func_data.parameter_amount != args.length) {
        eval_set_err(e, EVAL_ERR_FUNC_PARAM_NUMBER(func_data.parameter_amount, args.length));
        return err;
    }

    // Make sure the caller is of the correct type
    EvalData caller = func_eval_caller(e, function_node, func_data);
    if (eval_has_err(e)) {
        return err;
    }

    // Make sure the parameters are of the correct type
    func_eval_params(e, function_node, evaluated_args, func_data);
    if (eval_has_err(e)) {
        free_eval_data(&caller);
        return err;
    }

    return caller;
}
