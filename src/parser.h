#ifndef _PARSER_H
#define _PARSER_H

#include "src/lexer.h"
typedef enum {
    AST_BINARY,
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

        /// Multiply:
        /// <expr: lhs> * <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } multiply;

        /// Divide:
        /// <expr: lhs> / <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } divide;

        /// Modulo:
        /// <expr: lhs> % <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } modulo;

        /// Sum:
        /// <expr: lhs> + <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } sum;

        /// Difference:
        /// <expr: lhs> - <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } difference;

        /// Less than:
        /// <expr: lhs> < <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } less_than;

        /// Greater than:
        /// <expr: lhs> > <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } greater_than;

        /// Less than or equal to:
        /// <expr: lhs> <= <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } less_than_or_eq;

        /// Greater than or equal to:
        /// <expr: lhs> >= <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } greater_than_or_eq;

        /// Equal:
        /// <expr: lhs> == <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } equal;

        /// Not equal:
        /// <expr: lhs> != <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } not_equal;

        /// And:
        /// <expr: lhs> && <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } and;

        /// Or:
        /// <expr: lhs> || <expr: rhs>
        struct {
            struct ASTNode *lhs;
            struct ASTNode *rhs;
        } or ;

        /// Function call:
        /// <name: ident> <inner: [expr]>
        struct {
            Token name;
            Vec_ASTNode inner;
        } function;

        /// Closure body:
        /// |<args: [expr]>| <body: expr>
        struct {
            Vec_ASTNode args;
            struct ASTNode *body;
        } closure;

        /// Access chain:
        /// <ident | function>.*
        Vec_ASTNode access;

        /// Access chain:
        /// [<expr>, <expr>, ...]
        Vec_ASTNode list;

        /// Indexing a list:
        /// <array: expr>[<access: expr>]
        struct {
            struct ASTNode *array;
            struct ASTNode *access;
        } index;

        /// Field in a json object:
        /// <key: expr | string>: <value: expr>
        struct {
            struct ASTNode *key;
            struct ASTNode *value;
        } json_field;

        /// A json object:
        /// { <json_field>,* }
        Vec_ASTNode json_object;

    } inner;
} ASTNode;

#endif // _PARSER_H
