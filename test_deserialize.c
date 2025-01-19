#include "./json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define FLAGS JSON_NEW_LINES | JSON_COLOR

void test_simple() {
    assert(TEST("\"hello\"") != NULL);
    assert(TEST("\"hello") == NULL);
    assert(TEST("          \"hello\"          ") != NULL);
    assert(TEST("-10") != NULL);
    assert(TEST("--10") == NULL);
    assert(TEST("-10.0") != NULL);
    assert(TEST("-10.0.0") == NULL);
    assert(TEST("-.0") == NULL); // this might need to be fixed
    assert(TEST(".0") == NULL);  // this might need to be fixed
}

void test_lists() {
}

void test_structs() {
}

int main() {
    test_simple();
    test_lists();
    test_structs();
}
