#ifndef _PARSER_H
#define _PARSER_H

#include "src/lexer.h"
#include "vector.h"

typedef enum {
    AST_TYPE_PRIMARY,
    AST_TYPE_UNARY,
    AST_TYPE_BINARY,
    AST_TYPE_FUNCTION,
    AST_TYPE_CLOSURE,
    AST_TYPE_ACCESS,
    AST_TYPE_LIST,
    AST_TYPE_JSON_FIELD,
    AST_TYPE_JSON_OBJECT,
    AST_TYPE_GROUPING,

    AST_TYPE_FALSE,
    AST_TYPE_TRUE,
    AST_TYPE_NULL,
} ASTNodeType;

typedef Vec(struct ASTNode *) Vec_ASTNode;

// note that `expr` is just any AST node
typedef struct ASTNode {
    ASTNodeType type;
    Range range;
    union {
        /// Primitive value
        /// Strings ("foo"), numbers (1029), idents (foo_bar), etc
        Token_norange primary;

        /// Unary Operator:
        /// ("!" |"-") expr
        struct {
            TokenType operator;
            struct ASTNode *rhs;
        } unary;

        /// Binary operation:
        /// <expr: lhs> operator  <expr: rhs>
        struct {
            struct ASTNode *lhs;
            TokenType operator;
            struct ASTNode *rhs;
        } binary;

        /// A grouping of an expression (aka parenthesis):
        /// "(" expr ")"
        struct ASTNode *grouping;

        /// Access chain:
        /// expr "." (identifier | number)
        ///
        /// Can also be written as
        /// expr "[" expr "]"
        ///
        /// If `inner` is NULL, then there is no inner expression. This looks
        /// like `.map()` or `.foo`. A NULL inner expression will have the data
        /// be the input json when the program is invoked.
        struct {
            struct ASTNode *inner;
            struct ASTNode *accessor;
        } access;

        /// Function call:
        /// <expr: callee> "." <ident: function_name> "(" <(expr ",")*: args> ")"
        struct {
            struct ASTNode *callee;
            Token_norange function_name;
            Vec_ASTNode args;
        } function;

        /// Closure body:
        /// "|" (identifier ",")* "|" expr
        struct {
            Vec_ASTNode args;
            struct ASTNode *body;
        } closure;

        /// List:
        /// "[" (expr ",")* "]"
        Vec_ASTNode list;

        /// Field in a json object:
        /// expr ":" expr
        struct {
            struct ASTNode *key;
            struct ASTNode *value;
        } json_field;

        /// A json object:
        /// "{" (json_field",")* "}"
        Vec_ASTNode json_object;

    } inner;
} ASTNode;

typedef struct {
    ASTNode *node;
    char *error_message;
} ParseResult;

ParseResult ast_parse(char *);

void ast_free(ASTNode *);

#endif // _PARSER_H
