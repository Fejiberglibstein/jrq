#include "src/json.h"
#include "src/json_serde.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_FLAGS JSON_FLAG_SPACES
#define TAB_FLAGS JSON_FLAG_SPACES | JSON_FLAG_TAB

void test(char *expected, Json input, JsonSerializeFlags flags) {
    char *res = json_serialize(&input, flags);
    json_free(input);

    if (strncmp(res, expected, strlen(expected)) != 0) {
        printf("`%s` did not equal the expected `%s`\n", res, expected);
        assert(false);
    }

    free(res);
}

void test_primitives() {
    test("10", json_number(10), DEFAULT_FLAGS);
    test("-29.731", json_number(-29.731), DEFAULT_FLAGS);
    test("true", json_boolean(true), DEFAULT_FLAGS);
    test("false", json_boolean(false), DEFAULT_FLAGS);
    test("\"fooo\"", json_string("fooo"), DEFAULT_FLAGS);
    test("null", json_null(), DEFAULT_FLAGS);
}

void test_list() {
    test("[]", json_list(), DEFAULT_FLAGS);
    test("[true]", JSON_LIST(json_boolean(true)), DEFAULT_FLAGS);
    test("[10.2]", JSON_LIST(json_number(10.2)), DEFAULT_FLAGS);

    test(
        "[true, false, 10.2, \"blehg\"]",
        JSON_LIST(json_boolean(true), json_boolean(false), json_number(10.2), json_string("blehg")),
        DEFAULT_FLAGS
    );

    test(
        "[true, [], 10.2, [10, 2], \"blehg\"]",
        JSON_LIST(
            json_boolean(true),
            json_list(),
            json_number(10.2),
            JSON_LIST(json_number(10), json_number(2)),
            json_string("blehg")
        ),
        DEFAULT_FLAGS
    );

    test(
        "[\n"
        "  true, \n"
        "  [], \n"
        "  10.2, \n"
        "  [\n"
        "    10, \n"
        "    2\n"
        "  ], \n"
        "  \"blehg\"\n"
        "]",
        JSON_LIST(
            json_boolean(true),
            json_list(),
            json_number(10.2),
            JSON_LIST(json_number(10), json_number(2)),
            json_string("blehg")
        ),
        TAB_FLAGS
    );
}

void test_objects() {
    test("{}", json_object(), DEFAULT_FLAGS);

    test("{\"foo\": true}", JSON_OBJECT("foo", json_boolean(true)), DEFAULT_FLAGS);

    test(
        "{\"foo\": true, \"bar\": [{\"foo\": 10.2, \"bleh\": null}]}",
        JSON_OBJECT(
            "foo",
            json_boolean(true),
            "bar",
            JSON_LIST(JSON_OBJECT("foo", json_number(10.2), "bleh", json_null()))
        ),
        DEFAULT_FLAGS
    );

    test(
        "{\n"
        "  \"foo\": true, \n"
        "  \"bar\": [\n"
        "    \"blehh\", \n"
        "    {\n"
        "      \"foo\": 10.2, \n"
        "      \"bleh\": null\n"
        "    }\n"
        "  ]\n"
        "}",
        JSON_OBJECT(
            "foo",
            json_boolean(true),
            "bar",
            JSON_LIST(
                json_string("blehh"), JSON_OBJECT("foo", json_number(10.2), "bleh", json_null())
            )
        ),
        TAB_FLAGS
    );
}

int main() {
    test_primitives();
    test_list();
    test_objects();
}
