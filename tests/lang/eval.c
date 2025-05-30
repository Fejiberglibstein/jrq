#include "src/eval.h"
#include "src/json.h"
#include "src/json_serde.h"
#include "src/parser.h"
#include <assert.h>
#include <stdio.h>

bool test_eval(char *expr, Json input, Json expected) {
    printf("Testing `%s`\n", expr);
    ASTNode *node = ast_parse(expr).node;

    EvalResult result = eval(node, input);

    if (result.type == RES_ERR) {
        json_free(input);
        json_free(expected);
        printf("%s\n", result.err.err);
        fflush(stdout);
        free(result.err.err);
        return false;
    }
    Json json = result.json;

    bool r = json_equal(json, expected);

    if (r == false) {
        char *result_str = json_serialize(&json, 0);
        char *expected_str = json_serialize(&expected, 0);

        printf("`%s` does not equal expected `%s`\n", result_str, expected_str);

        free(result_str);
        free(expected_str);
    }

    json_free(input);
    json_free(expected);
    json_free(json);
    fflush(stdout);

    return r;
}

void simple_eval() {
    // Simple numbers and strings

    assert(test_eval("", json_number(10), json_number(10)));
    assert(test_eval("10 + 102", json_null(), json_number(112)));
    assert(test_eval("10 + 10 * 2", json_null(), json_number(30)));
    assert(test_eval("12 / 3 - 3", json_null(), json_number(1)));
    assert(test_eval("12 % 3", json_null(), json_number(0)));
    assert(test_eval("(12 - 3) * 4", json_null(), json_number(9 * 4)));
    assert(test_eval("true != false", json_null(), json_boolean(true)));
    assert(test_eval("12 >= 3", json_null(), json_boolean(true)));
    assert(test_eval("-(6-2)", json_null(), json_number(-4)));
    assert(test_eval("-2", json_null(), json_number(-2)));
    assert(test_eval("!(10 == 2)", json_null(), json_boolean(!(10 == 2))));
    assert(test_eval("4-6/2 <= (4-6)/2", json_null(), json_boolean(4 - 6 / 2 <= (4 - 6) / 2)));
    assert(test_eval("\"blehh\" == \"blehh\"", json_null(), json_boolean(true)));
    assert(test_eval("\"blehh\" != \"stupid\"", json_null(), json_boolean(true)));

    // objects and lists
    assert(test_eval(
        "[10, 4-2, 5 == 2]",
        json_null(),
        JSON_LIST(json_number(10), json_number(4 - 2), json_boolean(5 == 2))
    ));
    assert(test_eval("{\"foo\": 10}", json_null(), JSON_OBJECT("foo", json_number(10))));
    assert(test_eval("{\"foo\": 4-2*4}", json_null(), JSON_OBJECT("foo", json_number(-4))));
    assert(test_eval("{\"foo\": 10} == {\"foo\": 12-2}", json_null(), json_boolean(true)));
    assert(test_eval(
        "{\"foo\": [4-2, 0 == 0 && 1 + 2 == 3, {}], \"bar\": 10, \"bar\": 8 == 2}",
        json_null(),
        JSON_OBJECT(
            "foo",
            JSON_LIST(json_number(4 - 2), json_boolean(0 == 0 && 1 + 2 == 3), json_object_sized(0)),
            "bar",
            json_boolean(8 == 2)
        )
    ));
}

void accesor_eval() {
    assert(test_eval("[10].0", JSON_LIST(json_number(10)), json_number(10)));
    assert(test_eval("[10, [290, [465]]][4 - 3].1", json_null(), JSON_LIST(json_number(465))));
    assert(test_eval(".0", JSON_LIST(json_number(10)), json_number(10)));
    assert(test_eval(
        "[.0[0], .1]",
        JSON_LIST(JSON_LIST(json_number(10)), json_number(4)),
        JSON_LIST(json_number(10), json_number(4))
    ));
    assert(test_eval(
        "{ .0: 2*.1-2, .2: .0 == .2 && .3 }",
        JSON_LIST(json_string("fooo"), json_number(4), json_string("bharr"), json_boolean(true)),
        JSON_OBJECT("fooo", json_number(6), "bharr", json_boolean(false))
    ));
    assert(test_eval(
        "{\"bar\": .fooo > 4- 2}.bar", JSON_OBJECT("fooo", json_number(4)), json_boolean(true)
    ));
    assert(test_eval(
        "{\"bar\": .fooo != 4 - 2, {\"foo\": \"J\"}.foo: [10,[4].0, .fooo, .bh.0]}",
        JSON_OBJECT("fooo", json_string("bleh"), "bh", JSON_LIST(json_number(2))),
        JSON_OBJECT(
            "bar",
            json_boolean(true),
            "J",
            JSON_LIST(json_number(10), json_number(4), json_string("bleh"), json_number(2))
        )
    ));
}

void function_eval() {
    assert(test_eval(
        ".iter().map(|v| v * 2 - 2).collect()",
        JSON_LIST(json_number(10), json_number(2)),
        JSON_LIST(json_number(18), json_number(2))
    ));
    assert(test_eval(
        ".map(|v| v.f + 2).collect()",
        JSON_LIST(JSON_OBJECT("f", json_number(2)), JSON_OBJECT("f", json_number(1))),
        JSON_LIST(json_number(4), json_number(3))
    ));
    assert(test_eval(
        ".keys().map(|v| { v: 10 }).collect()",
        JSON_OBJECT("foo", json_null(), "bar", json_null(), "baz", json_null()),
        JSON_LIST(
            JSON_OBJECT("foo", json_number(10)),
            JSON_OBJECT("bar", json_number(10)),
            JSON_OBJECT("baz", json_number(10))
        )
    ));
    assert(test_eval(
        ".keys().map(|v| { v: 10}).collect()",
        JSON_OBJECT("foo", json_null(), "bar", json_null(), "baz", json_null()),
        JSON_LIST(
            JSON_OBJECT("foo", json_number(10)),
            JSON_OBJECT("bar", json_number(10)),
            JSON_OBJECT("baz", json_number(10))
        )
    ));
    assert(test_eval(
        ".map(|[k, v]| { v: k}).collect()",
        JSON_OBJECT(
            "foo", json_string("kys"), "bar", json_string("blurs"), "baz", json_string("hdfw")
        ),
        JSON_LIST(
            JSON_OBJECT("kys", json_string("foo")),
            JSON_OBJECT("blurs", json_string("bar")),
            JSON_OBJECT("hdfw", json_string("baz"))
        )
    ));
    assert(test_eval(
        ".map(|[[v, k], v2]| { k: v + v2}).collect().0 .hji",
        JSON_LIST(JSON_LIST(JSON_LIST(json_number(10), json_string("hji")), json_number(4))),
        json_number(14)
    ));
}

int main() {
    simple_eval();
    accesor_eval();
    function_eval();
}
