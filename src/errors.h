#ifndef _ERRORS_H
#define _ERRORS_H

#include "src/lexer.h"

#define ERROR_MISSING_RPAREN "Missing closing parenthesis ')'"
#define ERROR_MISSING_RBRACKET "Missing closing bracket ']'"
#define ERROR_MISSING_RBRACE "Missing closing brace '}'"
#define ERROR_MISSING_CLOSURE "Missing closing closure bar '|'"
#define ERROR_UNEXPECTED_TOKEN "Unexpected token"
#define ERROR_EXPECTED_IDENT "Expected identifier"
#define ERROR_EXPECTED_EOF "Expected eof"

// Json errors
#define ERROR_EXPECTED_STRING_OR_IDENT "Expected string or identifier key in json literal"
#define ERROR_EXPECTED_STRING "Expected string key in json literal"
#define ERROR_EXPECTED_COLON "Expected colon ':' after key in json literal"

// Error wrappers
#define TYPE_ERROR(...) "Type Error: " __VA_ARGS__
#define RUNTIME_ERROR(...) "Runtime Error: " __VA_ARGS__

// Eval errors
#define EVAL_ERR_UNARY_MINUS(t)                                                                    \
    TYPE_ERROR("Unexpected arguments to unary - (expected number, got %s)", t)

#define EVAL_ERR_UNARY_NOT(t)                                                                      \
    TYPE_ERROR("Unexpected arguments to unary ! (expected bool, got %s)", t)

#define EVAL_ERR_BINARY_OP(OP, exp, t)                                                             \
    TYPE_ERROR("Unexpected arguments to binary " OP " (expected %s, got %s)", exp, t)

#define EVAL_ERR_JSON_KEY_STRING(t) TYPE_ERROR("Expected string in json key, got %s", t)

#define EVAL_ERR_LIST_ACCESS(t) TYPE_ERROR("Expected number in list access, got %s", t)
#define EVAL_ERR_JSON_ACCESS(t) TYPE_ERROR("Expected string in object access, got %s", t)
#define EVAL_ERR_INNER_ACCESS(t) TYPE_ERROR("Can not index a %s", t)

#define EVAL_ERR_FUNC_NOT_FOUND(t) TYPE_ERROR("No function named %s", t)
#define EVAL_ERR_FUNC_PARAM_NUMBER(exp, act)                                                       \
    TYPE_ERROR(                                                                                    \
        "Too %s arguments to function (expected %d, have %zu)",                                    \
        exp > act ? "few" : "many",                                                                \
        exp,                                                                                       \
        act                                                                                        \
    )
#define EVAL_ERR_FUNC_NO_CLOSURE TYPE_ERROR("Invalid arguments to function (expected closure)")
#define EVAL_ERR_FUNC_CLOSURE_PARAM_NUMBER(exp, act)                                               \
    TYPE_ERROR(                                                                                    \
        "Too %s parameters in closure (expected %d, have %d)",                                     \
        exp > act ? "few" : "many",                                                                \
        exp,                                                                                       \
        act                                                                                        \
    )
#define EVAL_ERR_FUNC_UNEXPECTED_CLOSURE                                                           \
    TYPE_ERROR("Invalid arguments to function (unexpected closure)")
#define EVAL_ERR_FUNC_WRONG_ARGS(exp, act)                                                         \
    TYPE_ERROR("Invalid arguments to function (expected %s, got %s)", exp, act)
#define EVAL_ERR_FUNC_WRONG_CALLER(exp, act)                                                       \
    TYPE_ERROR("Invalid caller on function (expected %s, got %s)", exp, act)
#define EVAL_ERR_VAR_NOT_FOUND(t) RUNTIME_ERROR("Use of undeclared variable %s", t)

typedef struct {
    char *err;
    Range range;
} JrqError;

JrqError jrq_error(Range r, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif // _ERRORS_H
