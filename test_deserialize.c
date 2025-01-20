#include "./json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    int length;
    int capacity;
} IntBuffer;

extern char *validate_json(char *json, IntBuffer *int_buf);
extern char *skip_whitespace(char *json);

char *validate(char *json) {
    IntBuffer ints = (IntBuffer) {.data = malloc(4), .capacity = 4, .length = 0};
    json = validate_json(json, &ints);
    if (json == NULL) {
        return NULL;
    }

    json = skip_whitespace(json);
    if (*json != '\0') {
        return NULL;
    }

    free(ints.data);
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

void validate_int_buf(char *data, IntBuffer *buf, int *ints, int int_len);

void test_int_buf() {
    IntBuffer ints = (IntBuffer) {.data = malloc(4), .capacity = 4, .length = 0};
#define list(l...) (int[]) l, sizeof((int[])l) / 4
    validate_int_buf("1", &ints, list({}));
    validate_int_buf("true", &ints, list({}));
    validate_int_buf("[true]", &ints, list({2}));
    validate_int_buf("[]", &ints, list({1}));
    validate_int_buf("[null, \"foo\"]", &ints, list({3}));
    validate_int_buf(
        "[10, [1, 2, 3], [2, [4, 4, 4], 10, true], 10, [2], []]", &ints, list({7, 4, 5, 4, 2, 1})
    );
#undef list
}

int main() {
    test_validate_simple();
    test_validate_lists();
    test_validate_structs();

    test_int_buf();
}

void validate_int_buf(char *data, IntBuffer *buf, int *ints, int int_len) {
    validate_json(data, buf);

    if (int_len != buf->length) {
        printf("`%s` failed on length\n", data);
        assert(int_len == buf->length);
    }
    for (int i = 0; i < buf->length; i++) {
        if (buf->data[i] != ints[i]) {
            printf("`%s` failed on index %d: %d != %d\n", data, i, buf->data[i], ints[i]);
            assert(buf->data[i] == ints[i]);
        }
    }

    buf->length = 0;
}
