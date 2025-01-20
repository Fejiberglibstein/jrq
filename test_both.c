#include "./json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLAGS JSON_NO_COMPACT | JSON_COLOR
#define string(v) v, sizeof(v)

void test(char *initial_str, int str_len) {

    Json *json = json_deserialize(initial_str);
    if (str_len == 0) {
        assert(json == NULL);
        free(json);
        return;
    }

    char *ser = json_serialize(json, 0);
    if (strncmp(ser, initial_str, str_len) != 0) {
        printf("`%s` != `%s`\n", ser, initial_str);
        assert(ser == initial_str && "Strings didn't match");
    }

    free(ser);
    free(json);
}

void test_simple() {
    test(string("10"));
    test(string("true"));
    test(string("\"foo \""));
    test(string("10.200000"));
    test(string("null"));

    test("10n", 0);
    test("truen", 0);
    test("bleh", 0);
    test("\"fooo\\\"", 0);
}

void test_list() {
    test(string("[]"));
    test(string("[10]"));
    test(string("[10, 4]"));
    test(string("[10, 4, \"help me\", \"die\"]"));
    test(string("[true, [true, false]]"));
    test(string("[true, [true], [true, false]]"));
}

int main() {
    test_simple();
    printf("\n");
    test_list();
}
