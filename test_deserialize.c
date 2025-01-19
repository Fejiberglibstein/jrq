#include "./json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    int length;
    int capacity;
} IntBuffer;

extern char *validate_json(char *json, IntBuffer int_buf);
char *validate(char *json) {
    IntBuffer ints = (IntBuffer) {.data = malloc(4), .capacity = 4, .length = 0};
    json = validate_json(json, ints);
    free(ints.data);
    return json;
}

void test_validate_simple() {
    assert(validate("\"hello\"") != NULL);
    assert(validate("\"hello\"]") != NULL);
    assert(validate("\"he\\\"llo\"") != NULL);
    assert(validate("\"hello") == NULL);
    assert(validate("          \"hello\"          ") != NULL);

    assert(validate("10") != NULL);
    assert(validate("123456789.10]") != NULL);
    assert(validate("3") != NULL);
    assert(validate("-10     ") != NULL);
    assert(validate("--10") == NULL);
    assert(validate("         -10.0") != NULL);
    assert(validate("-10.0.0") == NULL);
    assert(validate("      -.0      ") != NULL);
    assert(validate(".0") != NULL);
    assert(validate(".") != NULL);

    assert(validate("true       ") != NULL);
    assert(validate("true]") != NULL);
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
}

int main() {
    test_validate_simple();
    test_validate_lists();
    test_validate_structs();
}
