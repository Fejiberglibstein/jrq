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

typedef struct {
    char *err;
    Range range;
} JrqError;

JrqError jrq_error(Range r, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif // _ERRORS_H
