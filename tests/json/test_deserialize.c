#include "../src/json.h"
#include "src/utils.h"
#include "src/vector.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef Vec(int) Vec_int;

typedef struct {
    Vec_int buf;
    // The size of all strings in the json
    int str_len;
    // Pointer to the region of memory we allocate to hold the strings
    char *str_ptr;
    // Pointer to the region of memory we allocate to hold all the json
    Json *arena;
} JsonData;

extern char *validate_json(char *json, JsonData *data);
char *skip_whitespace(char *json) {
    while (is_whitespace(*json)) {
        json++;
    }
    return json;
}

struct ret {
    char *end;
    int str_len;
};

struct ret validate(char *json) {
    JsonData data = {
        .buf = {.data = malloc(4), .capacity = 4, .length = 0},
        .str_len = 0,
        .arena = NULL,
        .str_ptr = NULL,
    };
    json = validate_json(json, &data);
    if (json == NULL) {
        free(data.buf.data);
        return (struct ret) {0};
    }

    json = skip_whitespace(json);
    if (*json != '\0') {
        free(data.buf.data);
        return (struct ret) {0};
    }

    free(data.buf.data);
    return (struct ret) {
        .end = json,
        .str_len = data.str_len,
    };
}

void test_validate_simple() {
    assert(validate("\"hello\"").end != NULL);
    assert(validate("\"hello\"").str_len == 6);
    assert(validate("\"he\\\"llo\"").end != NULL);
    assert(validate("          \"hello\"          ").end != NULL);
    assert(validate("\"hello\"]").end == NULL);
    assert(validate("\"hello  ").end == NULL);

    assert(validate("10").end != NULL);
    assert(validate("123456789.10]").end == NULL);
    assert(validate("3").end != NULL);
    assert(validate("-10     ").end != NULL);
    assert(validate("--10").end == NULL);
    assert(validate("         -10.0").end != NULL);
    assert(validate("-10.0.0").end == NULL);
    assert(validate("      -.0      ").end != NULL);
    assert(validate(".0").end != NULL);
    assert(validate(".").end != NULL);

    assert(validate("true       ").end != NULL);
    assert(validate("true]").end == NULL);
    assert(validate("         false").end != NULL);
    assert(validate("        null         ").end != NULL);
    assert(validate("hurwhui").end == NULL);
    assert(validate("h").end == NULL);

    assert(validate("").end == NULL);
}

void test_validate_lists() {
    assert(validate("[]").end != NULL);
    assert(validate("[true]").end != NULL);
    assert(validate("[0, 10, true, null]").end != NULL);
    assert(validate("[0, 10, true, \"hello\"]").end != NULL);
    assert(validate("[4, 3, 2,]").end != NULL);
    assert(validate("[4, [10, 2], \"hellllooo\", [4, 4,], ]").end != NULL);

    assert(validate("[4, \"hello]").end == NULL);
    assert(validate("[0, 10, true \"hello\"]").end == NULL);
    assert(validate("[4, 3, 2,,]").end == NULL);
    assert(validate("[4,, 3, 2]").end == NULL);
    assert(validate("[4, 3, 2").end == NULL);
    assert(validate("[[4, 3, 2], 3, 2").end == NULL);
    assert(validate("[[4, 3, 2, 3], 2").end == NULL);
}

void test_validate_structs() {
    assert(validate("{}").end != NULL);
    assert(validate("{\"foo\": true}").end != NULL);
    assert(validate("{     \"foo\"    : true  , }").end != NULL);
    assert(
        validate("{\"foo\": true, \"bleh\": {\"car\": [10, 2, 3], \"h\": 10}, \"bl\": null}").end
        != NULL
    );
    assert(
        validate("{\"foo\": true, \"bleh\": {\"car\": [10, 2, 3], \"h\": 10}, \"bl\": null}")
            .str_len
        == 18
    );

    assert(validate("{\"foo\": true]").end == NULL);
    assert(validate("{\"foo\": true]}").end == NULL);
    assert(validate("{\"foo\": true,,}").end == NULL);
    assert(validate("{true}").end == NULL);
    assert(validate("{\"foo\"}").end == NULL);
    assert(validate("{\"foo\": }").end == NULL);
}

void validate_int_buf(char *str, JsonData *data, int *ints, int int_len);

void test_int_buf() {
    JsonData data = {
        .buf = {.data = malloc(4), .capacity = 4, .length = 0},
        .str_len = 0,
        .arena = NULL,
        .str_ptr = NULL,
    };
#define list(l...) (int[]) l, sizeof((int[])l) / 4
    validate_int_buf("1", &data, list({}));
    validate_int_buf("true", &data, list({}));
    validate_int_buf("[true]", &data, list({2}));
    validate_int_buf("[]", &data, list({1}));
    validate_int_buf("[null, \"foo\"]", &data, list({3}));
    validate_int_buf(
        "[10, [1, 2, 3], [2, [4, 4, 4], 10, true], 10, [2], []]", &data, list({7, 4, 5, 4, 2, 1})
    );
#undef list
    free(data.buf.data);
}

int main() {
    test_validate_simple();
    test_validate_lists();
    test_validate_structs();

    test_int_buf();
}

void validate_int_buf(char *str, JsonData *data, int *ints, int int_len) {
    validate_json(str, data);

    if (int_len != data->buf.length) {
        // printf("`%s` failed on length\n", str);
        assert(int_len == data->buf.length);
    }
    for (int i = 0; i < data->buf.length; i++) {
        if (data->buf.data[i] != ints[i]) {
            // printf("`%s` failed on index %d: %d != %d\n", str, i, data->buf.data[i], ints[i]);
            assert(data->buf.data[i] == ints[i]);
        }
    }

    data->buf.length = 0;
}
