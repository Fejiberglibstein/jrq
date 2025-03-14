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

    ParseResult res = ast_parse(input);

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
    ast_free(res.node);
    if (err != NULL) {
        printf("%s\n", err);
        free(err);
        assert(false);
    }
}

void test_primary_expr() {

    test_parse("", NULL, NULL);

    ASTNode *initial = &(ASTNode) {
        .type = AST_TYPE_BINARY,

        .inner.binary.lhs = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token_norange) {
                .type = TOKEN_NUMBER,
                .inner.number = 10,
            },
        },

        .inner.binary.operator = TOKEN_MINUS,

        .inner.binary.rhs = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token_norange) {
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
            .inner.primary = (Token_norange) {
                .type = TOKEN_IDENT,
                .inner.ident = "ofoobar",
            },
        }
    });

    test_parse(" \"foo\" * (10 - 2  )", NULL, &(ASTNode) {
            .type = AST_TYPE_BINARY,

            .inner.binary.lhs = &(ASTNode) {
                .type = AST_TYPE_PRIMARY,
                .inner.primary = (Token_norange) {
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
        .inner.primary = (Token_norange) {
            .type = TOKEN_IDENT,
            .inner.ident = "foo",
        },
    };

    ASTNode *bar = &(ASTNode) {
        .type = AST_TYPE_PRIMARY,
        .inner.primary = (Token_norange) {
            .type = TOKEN_IDENT,
            .inner.ident = "bar",
        },
    };

    ASTNode *baz = &(ASTNode) {
        .type = AST_TYPE_PRIMARY,
        .inner.primary = (Token_norange) {
            .type = TOKEN_IDENT,
            .inner.ident = "baz",
        },
    };

    ASTNode *foo_bar = &(ASTNode) {
        .type = AST_TYPE_ACCESS,
        .inner.access.inner = foo,
        .inner.access.accessor = bar,
    };

    ASTNode *foo_bar_baz = &(ASTNode) {
        .type = AST_TYPE_ACCESS,
        .inner.access.inner = foo_bar,
        .inner.access.accessor = baz,
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
        .inner.function.callee = foo_bar,
        .inner.function.function_name = baz->inner.primary,
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
        .inner.function.callee = foo,
        .inner.function.function_name = bar->inner.primary,
    });

    test_parse("foo.bar(|| 10, 10, |foo, [bar, [baz], foo]| (foo.bar))", NULL, &(ASTNode) {
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
                            &(ASTNode) {
                                .type = AST_TYPE_LIST,
                                .inner.list = (Vec_ASTNode) {
                                    .data = (ASTNode*[]) {
                                        bar,
                                        &(ASTNode) {
                                            .type = AST_TYPE_LIST,
                                            .inner.list = (Vec_ASTNode) {
                                                .data = (ASTNode*[]) {
                                                    baz,
                                                },
                                                .length = 1,
                                            },
                                        },
                                        foo,
                                    },
                                    .length = 3,
                                },
                            },
                        },
                        .length = 2,
                    },
                    .inner.closure.body = &(ASTNode) {
                        .type = AST_TYPE_GROUPING,
                        .inner.grouping = foo_bar,
                    },
                }
            },
            .length = 3,
        },
        .inner.function.callee = foo,
        .inner.function.function_name = bar->inner.primary,
    });

    test_parse("foo.bar().baz", NULL, &(ASTNode) {
        .type = AST_TYPE_ACCESS,
        .inner.access.accessor = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token_norange) {
                .type = TOKEN_IDENT,
                .inner.ident = "baz",
            },
        },
        .inner.access.inner = &(ASTNode) {
            .type = AST_TYPE_FUNCTION,
            .inner.function.args = {
                .length = 0,
            },
            .inner.function.callee = foo,
            .inner.function.function_name = bar->inner.primary,
        },
    });
}

static void test_json_literals() {
    test_parse("{\"foo\": 10-2, \"bl\": 10}", NULL, &(ASTNode) {
        .type = AST_TYPE_JSON_OBJECT,
        .inner.json_object = (Vec_ASTNode) {
            .length = 2,
            .data = (ASTNode*[]) {
                &(ASTNode) {
                    .type = AST_TYPE_JSON_FIELD,
                    .inner.json_field.key = &(ASTNode) {
                        .type = AST_TYPE_PRIMARY,
                        .inner.primary = (Token_norange) {
                            .type = TOKEN_STRING,
                            .inner.string = "foo"
                        },
                    },
                    .inner.json_field.value = &(ASTNode) {
                        .type = AST_TYPE_BINARY,
                        .inner.binary.lhs = &(ASTNode) {
                            .type = AST_TYPE_PRIMARY,
                            .inner.primary = (Token_norange) {
                                .type = TOKEN_NUMBER,
                                .inner.number = 10,
                            },
                        },
                        .inner.binary.rhs = &(ASTNode) {
                            .type = AST_TYPE_PRIMARY,
                            .inner.primary = (Token_norange) {
                                .type = TOKEN_NUMBER,
                                .inner.number = 2,
                            },
                        },
                        .inner.binary.operator = TOKEN_MINUS,
                    },
                },
                &(ASTNode) {
                    .type = AST_TYPE_JSON_FIELD,
                    .inner.json_field.key = &(ASTNode) {
                        .type = AST_TYPE_PRIMARY,
                        .inner.primary = (Token_norange) {
                            .type = TOKEN_STRING,
                            .inner.string = "bl"
                        },
                    },
                    .inner.json_field.value = &(ASTNode) {
                        .type = AST_TYPE_PRIMARY,
                        .inner.primary = (Token_norange) {
                            .type = TOKEN_NUMBER,
                            .inner.number = 10,
                        },
                    },
                },
            },
        },
    });
    test_parse("{\"foo\": .foo + 2}.bleh", NULL, &(ASTNode) {
        .type = AST_TYPE_ACCESS,
        .inner.access.inner = &(ASTNode) {
            .type = AST_TYPE_JSON_OBJECT,
            .inner.json_object = (Vec_ASTNode) {
                .length = 1,
                .data = (ASTNode*[]) {
                    &(ASTNode) {
                        .type = AST_TYPE_JSON_FIELD,
                        .inner.json_field.key = &(ASTNode) {
                            .type = AST_TYPE_PRIMARY,
                            .inner.primary = (Token_norange) {
                                .type = TOKEN_STRING,
                                .inner.string = "foo"
                            },
                        },
                        .inner.json_field.value = &(ASTNode) {
                            .type = AST_TYPE_BINARY,
                            .inner.binary.lhs = &(ASTNode) {
                                .type = AST_TYPE_ACCESS,
                                .inner.access.inner = NULL,
                                .inner.access.accessor = &(ASTNode) {
                                    .type = AST_TYPE_PRIMARY,
                                    .inner.primary = (Token_norange) {
                                        .type = TOKEN_IDENT,
                                        .inner.ident = "foo",
                                    }
                                },
                            },
                            .inner.binary.rhs = &(ASTNode) {
                                .type = AST_TYPE_PRIMARY,
                                .inner.primary = (Token_norange) {
                                    .type = TOKEN_NUMBER,
                                    .inner.number = 2,
                                },
                            },
                            .inner.binary.operator = TOKEN_PLUS,
                        },
                    },
                },
            },
        },
        .inner.access.accessor = &(ASTNode) {
            .type = AST_TYPE_PRIMARY,
            .inner.primary = (Token_norange) {
                .type = TOKEN_IDENT,
                .inner.ident = "bleh",
            }
        },
    });
}

static void test_errors() {
    test_parse("foo .", ERROR_EXPECTED_IDENT, NULL);
    test_parse("foo 2", ERROR_EXPECTED_EOF, NULL);
    test_parse("foo + ", ERROR_UNEXPECTED_TOKEN, NULL);
    test_parse("foo[10", ERROR_MISSING_RBRACKET, NULL);
    test_parse("(foo (foo + bar)", ERROR_MISSING_RPAREN, NULL);
    test_parse(".bar(|)", ERROR_MISSING_CLOSURE, NULL);
}

int main() {
    test_primary_expr();
    test_errors();
    test_access_function_expr();
    test_json_literals();
}

static char *validate_ast_node(ASTNode *exp, ASTNode *act) {
    if (exp == NULL || act == NULL) {
        return (exp == act) ? NULL : "One was null the other wasn't";
    }
    jqr_assert(INT, exp, act, ->type);

    char *err;

    switch (exp->type) {
    case AST_TYPE_PRIMARY:
        // Silly fix because i made the parser turn idents into strings.
        if (!(exp->inner.primary.type == TOKEN_IDENT && act->inner.primary.type == TOKEN_STRING)) {
            jqr_assert(INT, exp, act, ->inner.primary.type);
        }

        switch (exp->inner.primary.type) {
        case TOKEN_IDENT:
            jqr_assert(STRING, exp, act, ->inner.primary.inner.ident);
            break;
        case TOKEN_STRING:
            jqr_assert(STRING, exp, act, ->inner.primary.inner.string);
            break;
        case TOKEN_NUMBER:
            jqr_assert(DOUBLE, exp, act, ->inner.primary.inner.number);
            break;
        default:
            break;
        };
        break;

    case AST_TYPE_UNARY:
        jqr_assert(INT, exp, act, ->inner.unary.operator);
        return validate_ast_node(exp->inner.unary.rhs, act->inner.unary.rhs);
    case AST_TYPE_BINARY:
        jqr_assert(INT, exp, act, ->inner.binary.operator);
        err = validate_ast_node(exp->inner.binary.rhs, act->inner.binary.rhs);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.binary.lhs, act->inner.binary.lhs);
    case AST_TYPE_FUNCTION:
        err = validate_ast_node(exp->inner.function.callee, act->inner.function.callee);
        if (err != NULL) {
            return err;
        }
        return validate_ast_list(exp->inner.function.args, act->inner.function.args);
    case AST_TYPE_CLOSURE:
        err = validate_ast_node(exp->inner.closure.body, act->inner.closure.body);
        if (err != NULL) {
            return err;
        }
        return validate_ast_list(exp->inner.closure.args, act->inner.closure.args);
    case AST_TYPE_ACCESS:
        err = validate_ast_node(exp->inner.access.accessor, act->inner.access.accessor);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.access.inner, act->inner.access.inner);
    case AST_TYPE_LIST:
        return validate_ast_list(exp->inner.list, act->inner.list);
    case AST_TYPE_JSON_FIELD:
        err = validate_ast_node(exp->inner.json_field.key, act->inner.json_field.key);
        if (err != NULL) {
            return err;
        }
        return validate_ast_node(exp->inner.json_field.value, act->inner.json_field.value);
    case AST_TYPE_JSON_OBJECT:
        return validate_ast_list(exp->inner.json_object, act->inner.json_object);
    case AST_TYPE_GROUPING:
        return validate_ast_node(exp->inner.grouping, act->inner.grouping);

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
