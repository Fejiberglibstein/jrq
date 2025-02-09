#include "src/json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_FLAGS JSON_FLAG_SPACES

void test(char *expected, Json *input, JsonSerializeFlags flags) {
    printf("testing %s\n", expected);

    char *res = json_serialize(input, flags);

    if (strcmp(res, expected) != 0) {
        printf("`%s` did not equal the expected `%s`\n", res, expected);
        assert(false);
    }

    free(res);
}

void test_primitives() {
    test("10", &(Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10}, DEFAULT_FLAGS);
    test("-29.731", &(Json) {.type = JSON_TYPE_NUMBER, .inner.number = -29.731}, DEFAULT_FLAGS);
    test("true", &(Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true}, DEFAULT_FLAGS);
    test("false", &(Json) {.type = JSON_TYPE_BOOL, .inner.boolean = false}, DEFAULT_FLAGS);
    test("\"fooo\"", &(Json) {.type = JSON_TYPE_STRING, .inner.string = "fooo"}, DEFAULT_FLAGS);
    test("null", &(Json) {.type = JSON_TYPE_NULL}, DEFAULT_FLAGS);
}

void test_list() {
    test("[]", &(Json) {.type = JSON_TYPE_LIST, .inner.list = {0}}, DEFAULT_FLAGS);

    test(
        "[true]",
        &(Json) {
            .type = JSON_TYPE_LIST,
            .inner.list = (JsonIterator){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true},
                },
                .length = 1,
            },
        },
        DEFAULT_FLAGS
    );

    test(
        "[10.2]",
        &(Json) {
            .type = JSON_TYPE_LIST,
            .inner.list = (JsonIterator){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10.2},
                },
                .length = 1,
            },
        },
        DEFAULT_FLAGS
    );

    test(
        "[true, false, 10.2, \"blehg\"]",
        &(Json) {
            .type = JSON_TYPE_LIST,
            .inner.list = (JsonIterator){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true},
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = false},
                    (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10.2},
                    (Json) {.type = JSON_TYPE_STRING, .inner.string = "blehg"},
                },
                .length = 4,
            },
        },
        DEFAULT_FLAGS
    );
}

int main() {
    test_primitives();
    test_list();
}
