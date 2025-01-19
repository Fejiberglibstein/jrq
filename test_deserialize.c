#include "./json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define FLAGS JSON_NEW_LINES | JSON_COLOR

void test_validate_simple() {
    assert(TEST("\"hello\"") != NULL);
    assert(TEST("\"he\\\"llo\"") != NULL);
    assert(TEST("\"hello") == NULL);
    assert(TEST("          \"hello\"          ") != NULL);
    assert(TEST("10") != NULL);
    assert(TEST("123456789.10") != NULL);
    assert(TEST("3") != NULL);
    assert(TEST("-10     ") != NULL);
    assert(TEST("--10") == NULL);
    assert(TEST("         -10.0") != NULL);
    assert(TEST("-10.0.0") == NULL);
    assert(TEST("      -.0      ") != NULL);
    assert(TEST(".0") != NULL);
    assert(TEST(".") != NULL);

    assert(TEST("true       ") != NULL);
    assert(TEST("         false") != NULL);
    assert(TEST("        null         ") != NULL);
    assert(TEST("hurwhui") == NULL);
    assert(TEST("h") == NULL);
}

void test_validate_lists() {
}

void test_validate_structs() {
}

int main() {
    test_validate_simple();
    test_validate_lists();
    test_validate_structs();
}
