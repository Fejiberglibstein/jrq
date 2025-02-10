#include "src/json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_FLAGS JSON_FLAG_SPACES
#define TAB_FLAGS JSON_FLAG_SPACES | JSON_FLAG_TAB

void test(char *expected, Json *input, JsonSerializeFlags flags) {
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
            .inner.list = (JsonList){
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
            .inner.list = (JsonList){
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
            .inner.list = (JsonList){
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

    test(
        "[true, [], 10.2, [10, 2], \"blehg\"]",
        &(Json) {
            .type = JSON_TYPE_LIST,
            .inner.list = (JsonList){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true},
                    (Json) {.type = JSON_TYPE_LIST, .inner.list = {0}},
                    (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10.2},
                    (Json) {.type = JSON_TYPE_LIST, .inner.list = {
                        .data = (Json[]) {
                            (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10},
                            (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 2},
                        },
                        .length = 2,
                    }},
                    (Json) {.type = JSON_TYPE_STRING, .inner.string = "blehg"},
                },
                .length = 5,
            },
        },
        DEFAULT_FLAGS
    );

    test(
        "[\n"
        "    true, \n"
        "    [], \n"
        "    10.2, \n"
        "    [\n"
        "        10, \n"
        "        2\n"
        "    ], \n"
        "    \"blehg\"\n"
        "]",
        &(Json) {
            .type = JSON_TYPE_LIST,
            .inner.list = (JsonList){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true},
                    (Json) {.type = JSON_TYPE_LIST, .inner.list = {0}},
                    (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10.2},
                    (Json) {.type = JSON_TYPE_LIST, .inner.list = {
                        .data = (Json[]) {
                            (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10},
                            (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 2},
                        },
                        .length = 2,
                    }},
                    (Json) {.type = JSON_TYPE_STRING, .inner.string = "blehg"},
                },
                .length = 5,
            },
        },
        TAB_FLAGS
    );
}

void test_objects() {
    test("{}", &(Json) {.type = JSON_TYPE_OBJECT, .inner.list = {0}}, DEFAULT_FLAGS);

    test(
        "{\"foo\": true}",
        &(Json) {
            .type = JSON_TYPE_OBJECT,
            .inner.list = (JsonList){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true, .field_name = "foo"},
                },
                .length = 1,
            },
        },
        DEFAULT_FLAGS
    );

    test(
        "{\"foo\": true, \"bar\": [{\"foo\": 10.2, \"bleh\": null}]}",
        &(Json) {
            .type = JSON_TYPE_OBJECT,
            .inner.list = (JsonList){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true, .field_name = "foo"},
                    (Json) {
                        .field_name = "bar", 
                        .type = JSON_TYPE_LIST, 
                        .inner.list = (JsonList) {
                            .length = 1,
                            .data = (Json[]) {
                                (Json) {
                                    .type = JSON_TYPE_OBJECT, 
                                    .inner.object = (JsonList) {
                                        .length = 2,
                                        .data = (Json[]) {
                                            (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10.2, .field_name = "foo"},
                                            (Json) {.type = JSON_TYPE_NULL,  .field_name = "bleh"},
                                        }
                                    },
                                },
                            },
                        },
                    },
                },
                .length = 2,
            },
        },
        DEFAULT_FLAGS
    );

    test(
        "{\n"
        "    \"foo\": true, \n"
        "    \"bar\": [\n"
        "        \"blehh\", \n"
        "        {\n"
        "            \"foo\": 10.2, \n"
        "            \"bleh\": null\n"
        "        }\n"
        "    ]\n"
        "}",
        &(Json) {
            .type = JSON_TYPE_OBJECT,
            .inner.list = (JsonList){
                .data = (Json[]) {
                    (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true, .field_name = "foo"},
                    (Json) {
                        .field_name = "bar", 
                        .type = JSON_TYPE_LIST, 
                        .inner.list = (JsonList) {
                            .length = 2,
                            .data = (Json[]) {
                                (Json) {.type = JSON_TYPE_STRING, .inner.string = "blehh"},
                                (Json) {
                                    .type = JSON_TYPE_OBJECT, 
                                    .inner.object = (JsonList) {
                                        .length = 2,
                                        .data = (Json[]) {
                                            (Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10.2, .field_name = "foo"},
                                            (Json) {.type = JSON_TYPE_NULL,  .field_name = "bleh"},
                                        }
                                    },
                                },
                            },
                        },
                    },
                },
                .length = 2,
            },
        },
        TAB_FLAGS
    );
}

int main() {
    test_primitives();
    test_list();
    test_objects();
}
