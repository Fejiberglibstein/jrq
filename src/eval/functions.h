#ifndef _EVAL_FUNCTIONS_H
#define _EVAL_FUNCTIONS_H

#include "src/eval/private.h"
#include "src/json.h"

// Don't use this with a T of any, instead just use the normal JSON_TYPE_LIST value
#define JSON_TYPE_LIST_T(v) (JSON_TYPE_LIST + v)
#define JSON_TYPE_ITERATOR (-1)
#define JSON_TYPE_CLOSURE (-10)
#define JSON_TYPE_CLOSURE_WITH_PARAMS(v) (JSON_TYPE_CLOSURE - (v))

/// Used to define what types a function expects
///
/// <number>.add(number)
/// The function_data for the previous function would have this form
/// {
///     .function_name    = "add",
///     .caller_type      = JSON_TYPE_NUMBER,
///     .parameter_types  = (JsonType[]) {JSON_TYPE_NUMBER},
///     .parameter_amount = 1,
/// }
///
/// In addition to all the normal json types, there is also the following json types you can use:
/// - JSON_TYPE_ANY
/// - JSON_TYPE_ITERATOR
/// - JSON_TYPE_LIST_T                   (list that must be a specific type)
/// - JSON_TYPE_CLOSURE                  (closure with no parameters) -> ||
/// - JSON_TYPE_CLOSURE_WITH_PARAMS(N)   (closure with N parameters)  -> |p1, p2, ..., pN|
struct function_data {
    /// The name of the function
    char *function_name;
    /// The expected type of the caller of the function.
    ///
    /// In `[10, 2].foo()`, the caller is of type JSON_TYPE_LIST
    JsonType caller_type;
    /// The expected type of each parameter to the function in a list.
    ///
    /// In `.foo(10, "grb")`, the parameter types would be [number, string]
    JsonType *parameter_types;
    /// The amount of parameters this function expects.
    uint parameter_amount;
};

EvalData func_expect_args(Eval *, ASTNode *, Json *, struct function_data);
int vs_push_closure_variable(Eval *e, ASTNode *var, Json value);
int vs_pop_closure_variable(Eval *e, ASTNode *var, Json value);
void vs_push_variable(VariableStack *vs, String var_name, Json value);
void vs_pop_variable(VariableStack *vs, String var_name);
Json vs_get_variable(Eval *e, String var_name);

#endif // _EVAL_FUNCTIONS_H
