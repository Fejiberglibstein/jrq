#include "./json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    int length;
    int capacity;
} IntBuffer;

typedef struct {
    IntBuffer buf;
    // The size of all strings in the json
    int str_len;
    // Pointer to the region of memory we allocate to hold the strings
    char *str_ptr;
    // Pointer to the region of memory we allocate to hold all the json
    Json *arena;
} JsonData;

extern char *validate_json(char *json, JsonData *data);
extern char *skip_whitespace(char *json);

char *validate(char *json) {
    JsonData data = {
        .buf = {.data = malloc(4), .capacity = 4, .length = 0},
        .str_len = 0,
        .arena = NULL,
        .str_ptr = NULL,
    };
    json = validate_json(json, &data);
    if (json == NULL) {
        return NULL;
    }

    json = skip_whitespace(json);
    if (*json != '\0') {
        return NULL;
    }

    free(data.buf.data);
    return json;
}

void test_validate_simple() {
    assert(validate("\"hello\"") != NULL);
    assert(validate("\"he\\\"llo\"") != NULL);
    assert(validate("          \"hello\"          ") != NULL);
    assert(validate("\"hello\"]") == NULL);
    assert(validate("\"hello  ") == NULL);

    assert(validate("10") != NULL);
    assert(validate("123456789.10]") == NULL);
    assert(validate("3") != NULL);
    assert(validate("-10     ") != NULL);
    assert(validate("--10") == NULL);
    assert(validate("         -10.0") != NULL);
    assert(validate("-10.0.0") == NULL);
    assert(validate("      -.0      ") != NULL);
    assert(validate(".0") != NULL);
    assert(validate(".") != NULL);

    assert(validate("true       ") != NULL);
    assert(validate("true]") == NULL);
    assert(validate("         false") != NULL);
    assert(validate("        null         ") != NULL);
    assert(validate("hurwhui") == NULL);
    assert(validate("h") == NULL);

    assert(validate("") == NULL);
}

void test_validate_lists() {
    assert(validate("[]") != NULL);
    assert(validate("[true]") != NULL);
    assert(validate("[0, 10, true, null]") != NULL);
    assert(validate("[0, 10, true, \"hello\"]") != NULL);
    assert(validate("[4, 3, 2,]") != NULL);
    assert(validate("[4, [10, 2], \"hellllooo\", [4, 4,], ]") != NULL);

    assert(validate("[4, \"hello]") == NULL);
    assert(validate("[0, 10, true \"hello\"]") == NULL);
    assert(validate("[4, 3, 2,,]") == NULL);
    assert(validate("[4,, 3, 2]") == NULL);
    assert(validate("[4, 3, 2") == NULL);
    assert(validate("[[4, 3, 2], 3, 2") == NULL);
    assert(validate("[[4, 3, 2, 3], 2") == NULL);
}

void test_validate_structs() {
    assert(validate("{}") != NULL);
    assert(validate("{\"foo\": true}") != NULL);
    assert(validate("{     \"foo\"    : true  , }") != NULL);
    assert(
        validate("{\"foo\": true, \"bleh\": {\"car\": [10, 2, 3], \"h\": 10}, \"bl\": null}") !=
        NULL
    );

    assert(validate("{\"foo\": true]") == NULL);
    assert(validate("{\"foo\": true]}") == NULL);
    assert(validate("{\"foo\": true,,}") == NULL);
    assert(validate("{true}") == NULL);
    assert(validate("{\"foo\"}") == NULL);
    assert(validate("{\"foo\": }") == NULL);
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
        printf("`%s` failed on length\n", str);
        assert(int_len == data->buf.length);
    }
    for (int i = 0; i < data->buf.length; i++) {
        if (data->buf.data[i] != ints[i]) {
            printf("`%s` failed on index %d: %d != %d\n", str, i, data->buf.data[i], ints[i]);
            assert(data->buf.data[i] == ints[i]);
        }
    }

    data->buf.length = 0;
}
