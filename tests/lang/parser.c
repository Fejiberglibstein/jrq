#include "src/parser.h"
#include "../test.h"
#include "src/errors.h"
#include "src/lexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char *validate_ast_node(ASTNode *exp, ASTNode *actual);
static char *validate_ast_list(Vec_ASTNode exp, Vec_ASTNode actual);

static void test_parse(char *input, char *expected_err, ASTNode *exp) {
    printf("Testing '%s'\n", input);

    Lexer l = lex_init(input);

    ParseResult res = ast_parse(&l);

    if (expected_err != NULL) {
        if (strcmp(expected_err, res.error_message) != 0) {
            printf("'%s' should equal '%s'\n", expected_err, res.error_message);
            assert(false);
        }
        return;
    }

    if (res.error_message != NULL) {
        printf("%s\n", res.error_message);
        assert(false);
    }

    char *err = validate_ast_node(exp, res.node);
    if (err != NULL) {
        printf("%s\n", expected_err);
        free(expected_err);
        assert(false);
    }
}

void test_primary_expr() {

    ASTNode *initial = &(ASTNode) {
        .type = AST_TYPE_BINARY,

        .inner.binary.lhs = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_NUMBER,
                .inner.number = 10,
            },
        },

        .inner.binary.operator = TOKEN_MINUS,

        .inner.binary.rhs = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_NUMBER,
                .inner.number = 2,
            },
        }

    };

    test_parse("10 - 2", NULL, initial);

    test_parse(" 10    -2==  ofoobar ", NULL, &(ASTNode) {
        .type = AST_TYPE_BINARY,

        .inner.binary.lhs = initial,
        .inner.binary.operator = TOKEN_EQUAL,

        .inner.binary.rhs = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_IDENT,
                .inner.ident = "ofoobar",
            },
        }
    });

    test_parse(" \"foo\" * (10 - 2  )", NULL, &(ASTNode) {
            .type = AST_TYPE_BINARY,

            .inner.binary.lhs = &(ASTNode) {
                .type = AST_TYPE_PRIMARY,
                .inner.primary = (Token) {
                    .type = TOKEN_STRING,
                    .inner.string = "foo",
                },
            },

            .inner.binary.operator = TOKEN_ASTERISK,

            .inner.binary.rhs = &(ASTNode) {
                .type = AST_TYPE_GROUPING,
                .inner.grouping = initial,
            },
    });
}

static void test_access_function_expr() {
    ASTNode *foo = &(ASTNode) {
        .type = AST_TYPE_PRIMARY,
        .inner.primary = (Token) {
            .type = TOKEN_IDENT,
            .inner.ident = "foo",
        },
    };

    ASTNode *foo_bar = &(ASTNode) {
        .type = AST_TYPE_ACCESS,
        .inner.access.inner = foo,
        .inner.access.accessor = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_IDENT,
                .inner.ident = "bar",
            },

        }
    };

    ASTNode *foo_bar_baz = &(ASTNode) {
        .type = AST_TYPE_ACCESS,
        .inner.access.inner = foo_bar,
        .inner.access.accessor = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_IDENT,
                .inner.ident = "baz",
            },
        },
    };

    ASTNode *ten = &(ASTNode) {
        .type = AST_TYPE_PRIMARY,
        .inner.primary.type = TOKEN_NUMBER,
        .inner.primary.inner.number = 10,
    };

    test_parse("foo", NULL, foo);
    test_parse("foo.bar", NULL, foo_bar);
    test_parse("foo.bar.baz", NULL, foo_bar_baz);

    test_parse("foo.bar.baz()", NULL, &(ASTNode) {
        .type = AST_TYPE_FUNCTION,
        .inner.function.args = (Vec_ASTNode) {
            .length = 0,
        },
        .inner.function.callee = foo_bar_baz,
    });

    test_parse("foo.bar(foo, foo.bar, 10)", NULL, &(ASTNode) {
        .type = AST_TYPE_FUNCTION,
        .inner.function.args = (Vec_ASTNode) {
            .data = (ASTNode*[]) {
                foo,
                foo_bar,
                ten,
            },
            .length = 3,
        },
        .inner.function.callee = foo_bar,
    });

    test_parse("foo.bar(|| 10, 10, |foo| (foo.bar))", NULL, &(ASTNode) {
        .type = AST_TYPE_FUNCTION,
        .inner.function.args = (Vec_ASTNode) {
            .data = (ASTNode*[]) {
                &(ASTNode) {
                    .type = AST_TYPE_CLOSURE,
                    .inner.closure.args = (Vec_ASTNode) {
                        .length = 0,
                    },
                    .inner.closure.body = ten,
                },
                ten,
                &(ASTNode) {
                    .type = AST_TYPE_CLOSURE,
                    .inner.closure.args = (Vec_ASTNode) {
                        .data = (ASTNode*[]) {
                            foo,
                        },
                        .length = 1,
                    },
                    .inner.closure.body = &(ASTNode) {
                        .type = AST_TYPE_GROUPING,
                        .inner.grouping = foo_bar,
                    },
                }
            },
            .length = 3,
        },
        .inner.function.callee = foo_bar,
    });

    test_parse("foo.bar().baz", NULL, &(ASTNode) {
        .type = AST_TYPE_ACCESS,
        .inner.access.accessor = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token) {
                .type = TOKEN_IDENT,
                .inner.ident = "baz",
            },
        },
        .inner.access.inner = &(ASTNode) {
            .type = AST_TYPE_FUNCTION,
            .inner.function.args = {
                .length = 0,
            },
            .inner.function.callee = foo_bar,
        },
    });
}

static void test_json_literals() {
    // todo: i should probably add tests
}

static void test_errors() {
    test_parse("foo .", ERROR_EXPECTED_IDENT, NULL);
    test_parse("foo 2", ERROR_EXPECTED_EOF, NULL);
    test_parse("foo + ", ERROR_UNEXPECTED_TOKEN, NULL);
    test_parse("foo[10", ERROR_MISSING_RBRACKET, NULL);
    test_parse("(foo (foo + bar)", ERROR_MISSING_RPAREN, NULL);
    test_parse("bar(|)", ERROR_MISSING_CLOSURE, NULL);
}

int main() {
    test_primary_expr();
    test_errors();
    test_access_function_expr();
    test_json_literals();
}

static char *validate_ast_node(ASTNode *exp, ASTNode *actual) {
    jqr_assert(INT, exp, actual, ->type);

    char *err;

    switch (exp->type) {
    case AST_TYPE_PRIMARY:
        jqr_assert(INT, exp, actual, ->inner.primary.type);

        switch (exp->inner.primary.type) {
        case TOKEN_IDENT:
            jqr_assert(STRING, exp, actual, ->inner.primary.inner.ident);
            break;
        case TOKEN_STRING:
            jqr_assert(STRING, exp, actual, ->inner.primary.inner.string);
            break;
        case TOKEN_NUMBER:
            jqr_assert(DOUBLE, exp, actual, ->inner.primary.inner.number);
            break;
        default:
            break;
        };
        break;

    case AST_TYPE_UNARY:
        jqr_assert(INT, exp, actual, ->inner.unary.operator);
        return validate_ast_node(exp->inner.unary.rhs, actual->inner.unary.rhs);
    case AST_TYPE_BINARY:
        jqr_assert(INT, exp, actual, ->inner.binary.operator);
        err = validate_ast_node(exp->inner.binary.rhs, actual->inner.binary.rhs);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.binary.lhs, actual->inner.binary.lhs);
    case AST_TYPE_FUNCTION:
        err = validate_ast_node(exp->inner.function.callee, actual->inner.function.callee);
        if (err != NULL) {
            return err;
        }
        return validate_ast_list(exp->inner.function.args, actual->inner.function.args);
    case AST_TYPE_CLOSURE:
        err = validate_ast_node(exp->inner.closure.body, actual->inner.closure.body);
        if (err != NULL) {
            return err;
        }
        return validate_ast_list(exp->inner.closure.args, actual->inner.closure.args);
    case AST_TYPE_ACCESS:
        err = validate_ast_node(exp->inner.access.accessor, actual->inner.access.accessor);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.access.inner, actual->inner.access.inner);
    case AST_TYPE_LIST:
        return validate_ast_list(exp->inner.list, actual->inner.list);
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
    jqr_assert(INT, exp, actual, .length);
    for (int i = 0; i < exp.length; i++) {
        char *err = validate_ast_node(exp.data[i], actual.data[i]);
        if (err != NULL) {
            return err;
        }
    }
    return NULL;
}
