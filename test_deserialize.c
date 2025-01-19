#include "./json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern char *TEST(char *);

void test_validate_simple() {
    assert(TEST("\"hello\"") != NULL);
    assert(TEST("\"hello\"]") != NULL);
    assert(TEST("\"he\\\"llo\"") != NULL);
    assert(TEST("\"hello") == NULL);
    assert(TEST("          \"hello\"          ") != NULL);

    assert(TEST("10") != NULL);
    assert(TEST("123456789.10]") != NULL);
    assert(TEST("3") != NULL);
    assert(TEST("-10     ") != NULL);
    assert(TEST("--10") == NULL);
    assert(TEST("         -10.0") != NULL);
    assert(TEST("-10.0.0") == NULL);
    assert(TEST("      -.0      ") != NULL);
    assert(TEST(".0") != NULL);
    assert(TEST(".") != NULL);

    assert(TEST("true       ") != NULL);
    assert(TEST("true]") != NULL);
    assert(TEST("         false") != NULL);
    assert(TEST("        null         ") != NULL);
    assert(TEST("hurwhui") == NULL);
    assert(TEST("h") == NULL);

    assert(TEST("") == NULL);
}

void test_validate_lists() {
    assert(TEST("[]") != NULL);
    assert(TEST("[true]") != NULL);
    assert(TEST("[0, 10, true, null]") != NULL);
    assert(TEST("[0, 10, true, \"hello\"]") != NULL);
    assert(TEST("[4, 3, 2,]") != NULL);
    assert(TEST("[4, [10, 2], null, [4, 4,], ]") != NULL);

    assert(TEST("[0, 10, true \"hello\"]") == NULL);
    assert(TEST("[4, 3, 2,,]") == NULL);
    assert(TEST("[4,, 3, 2]") == NULL);
}

void test_validate_structs() {
}

int main() {
    test_validate_simple();
    test_validate_lists();
    test_validate_structs();
}
