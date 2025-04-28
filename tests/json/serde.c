#include "../src/json.h"
#include "src/errors.h"
#include "src/json_serde.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define string(v) v, sizeof(v)
#define all(v) v, v, sizeof(v)

void test(char *input, char *expected, int str_len) {
    printf("Testing %s\n", input);
    DeserializeResult res = json_deserialize(input);
    if (str_len == 0) {
        assert(res.type == RES_ERR);
        free(res.err.err);
        return;
    }

    char *ser = json_serialize(&res.result, JSON_FLAG_SPACES);
    json_free(res.result);
    if (strncmp(ser, expected, str_len) != 0) {
        printf("`%s` != `%s`\n", ser, expected);
        assert(ser == expected && "Strings didn't match");
    }

    free(ser);
}

void test_simple() {
    test("   10", string("10"));
    test("true    ", string("true"));
    test("\"foo \"   ", string("\"foo \""));
    test("   10.200000   ", string("10.2"));
    test("   -492   ", string("-492"));
    test("null  ", string("null"));

    test("10n", NULL, 0);
    test("10 0", NULL, 0);
    test("true n", NULL, 0);
    test("bleh", NULL, 0);
    test("\"fooo\\\"", NULL, 0);
    test("\"fooo\"     h", NULL, 0);
}

void test_list() {
    test("  []    ", string("[]"));
    test("  [10]   ", string("[10]"));
    test("[   10,4  ]  ", string("[10, 4]"));
    test(" [    true   ,[  10 ]] ", string("[true, [10]]"));
    test(all("[10, 4, \"help me\", \"die\"]"));
    test(all("[true, []]"));
    test(all("[true, [true, false]]"));
    test(all("[true, false, [true], false]"));
    test(all("[true, false, [1], [true, false]]"));
    test(all("[true, [1, [2, 3], 4, 5], [true, false]]"));
    test(all("[true, [1, [2, 3], 4, [-3, 2], 4], [true, false]]"));
}

void test_struct() {
    test("   {} ", string("{}"));
    test("{   \" hhhi\"  :  10  }", string("{\" hhhi\": 10}"));
    test("{   \" hhhi\"  :  10 ,\"10\" :null}", string("{\" hhhi\": 10, \"10\": null}"));
    test(all("{\"foo\": 10, \"fooo\": {}}"));
    test(
        all("{\"foo\": 10, \"bleh\": {\"in\": null, \"out\": {\"sh\": false}, \"hi\": null, "
            "\"hi2\": {}, \"j\": 3}, \"h\": -1}")
    );
}

void test_list_and_struct() {
    test(all("[10, {}, [], {\"nehg\": null}]"));
    test(all("{\"foo\": {}, \"grg\": -20, \"bh\": [10, 9, 2, [3, 5, {\"hi\": [4]}, 8, 3], 2, 8]}"));
    test("   {\"foo\": 10, \"foo\": 2} ", string("{\"foo\": 2}"));
}

int main() {
    test_simple();
    printf("\n");
    test_list();
    printf("\n");
    test_struct();
    printf("\n");
    test_list_and_struct();
}
