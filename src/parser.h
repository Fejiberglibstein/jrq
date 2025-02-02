#ifndef _PARSER_H
#define _PARSER_H

#include "src/lexer.h"
typedef enum {
    AST_TYPE_PRIMARY,
    AST_TYPE_NOT,
    AST_TYPE_NEGATIVE,
    AST_TYPE_BINARY,
    AST_TYPE_FUNCTION,
    AST_TYPE_CLOSURE,
    AST_TYPE_ACCESS,
    AST_TYPE_LIST,
    AST_TYPE_INDEX,
    AST_TYPE_JSON_FIELD,
    AST_TYPE_JSON_OBJECT,

} ASTNodeType;

typedef struct {
    struct ASTNode *data;
    int length;
    int capacity;
} Vec_ASTNode;

// note that `expr` is just any AST node
typedef struct ASTNode {
    ASTNodeType type;
    union {
        /// Primitive value
        /// Strings ("foo"), numbers (1029), idents (foo_bar), etc
        Token primary;

        /// Not:
        /// !<expr>
        struct ASTNode * not;

        /// Negate:
        /// -<expr>
        struct ASTNode *negative;

        /// Binary operation:
        /// <expr: lhs> operator  <expr: rhs>
        struct {
            struct ASTNode *lhs;
            TokenType operator;
            struct ASTNode *rhs;
        } binary;

        /// Function call:
        /// identifier "(" (expr ",")* ")"
        struct {
            Token name;
            Vec_ASTNode inner;
        } function;

        /// Closure body:
        /// "|" (identifier ",")* "|" expr
        struct {
            Vec_ASTNode args;
            struct ASTNode *body;
        } closure;

        /// Access chain:
        /// primary ( "." identifier)*
        Vec_ASTNode access;

        /// List:
        /// "[" (expr ",")* "]"
        Vec_ASTNode list;

        /// Indexing a list:
        /// expr "[" expr "]"
        struct {
            struct ASTNode *array;
            struct ASTNode *access;
        } index;

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
    ASTNode node;
    char *error_message;
} ParseResult;

ParseResult ast_parse(Lexer *l);

#endif // _PARSER_H
