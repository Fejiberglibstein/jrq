#include "src/parser.h"
#include "../test.h"
#include "src/lexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char *validate_ast_node(ASTNode *exp, ASTNode *actual);
static char *validate_ast_list(Vec_ASTNode exp, Vec_ASTNode actual);

static bool parse(char *input, ASTNode *exp) {
    Lexer l = lex_init("10 - 2");

    ParseResult res = ast_parse(&l);
    assert(res.error_message == NULL);

    char *err = validate_ast_node(exp, res.node);
    if (err != NULL) {
        printf("%s", err);
        free(err);
        return false;
    }
    return true;
}

void test_simple_expr() {

    parse("10 - 2", &(ASTNode) {
        .type = AST_TYPE_BINARY,
        .inner.binary.operator = TOKEN_MINUS,

        .inner.binary.lhs = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_INT,
                .inner.Int = 10,
            },
        },

        .inner.binary.rhs = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_INT,
                .inner.Int = 10,
            },
        }
    });
}

int main() {
    test_simple_expr();
}

static char *validate_ast_node(ASTNode *exp, ASTNode *actual) {
    jaq_assert(INT, exp, actual, ->type);

    char *err;

    switch (exp->type) {
    case AST_TYPE_PRIMARY:
        jaq_assert(INT, exp, actual, ->inner.primary.type);

        switch (exp->inner.primary.type) {
        case TOKEN_IDENT:
            jaq_assert(STRING, exp, actual, ->inner.primary.inner.ident);
            break;
        case TOKEN_STRING:
            jaq_assert(STRING, exp, actual, ->inner.primary.inner.string);
            break;
        case TOKEN_DOUBLE:
            jaq_assert(DOUBLE, exp, actual, ->inner.primary.inner.Double);
            break;
        case TOKEN_INT:
            jaq_assert(INT, exp, actual, ->inner.primary.inner.Int);
            break;
        default:
            break;
        };
        break;

    case AST_TYPE_UNARY:
        jaq_assert(INT, exp, actual, ->inner.unary.operator);
        return validate_ast_node(exp->inner.unary.rhs, actual->inner.unary.rhs);
    case AST_TYPE_BINARY:
        jaq_assert(INT, exp, actual, ->inner.binary.operator);
        err = validate_ast_node(exp->inner.binary.rhs, actual->inner.binary.rhs);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.binary.lhs, actual->inner.binary.lhs);
    case AST_TYPE_FUNCTION:
        jaq_assert(STRING, exp, actual, ->inner.function.name.inner.ident);
        return validate_ast_list(exp->inner.function.args, actual->inner.function.args);
    case AST_TYPE_CLOSURE:
        err = validate_ast_node(exp->inner.closure.body, actual->inner.closure.body);
        if (err != NULL) {
            return err;
        }
        return validate_ast_list(exp->inner.closure.args, actual->inner.closure.args);
    case AST_TYPE_ACCESS:
        return validate_ast_list(exp->inner.access, actual->inner.access);
    case AST_TYPE_LIST:
        return validate_ast_list(exp->inner.list, actual->inner.list);
    case AST_TYPE_INDEX:
        err = validate_ast_node(exp->inner.index.access, actual->inner.index.access);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.index.array, actual->inner.index.array);
    case AST_TYPE_JSON_FIELD:
        err = validate_ast_node(exp->inner.json_field.key, actual->inner.json_field.key);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.json_field.value, actual->inner.json_field.value);
    case AST_TYPE_JSON_OBJECT:
        return validate_ast_list(exp->inner.json_object, actual->inner.json_object);
    case AST_TYPE_GROUPING:
        return validate_ast_node(exp->inner.grouping, actual->inner.grouping);

        // All of these are only types so we can ignore them
    case AST_TYPE_FALSE:
    case AST_TYPE_TRUE:
    case AST_TYPE_NULL:
        break;
    }

    return NULL;
}

static char *validate_ast_list(Vec_ASTNode exp, Vec_ASTNode actual) {
    jaq_assert(INT, exp, actual, .length);
    for (int i = 0; i < exp.length; i++) {
        char *err = validate_ast_node(&exp.data[i], &actual.data[i]);
        if (err != NULL) {
            return err;
        }
    }
    return NULL;
}
