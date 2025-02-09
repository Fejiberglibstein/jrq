#include "src/json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test(char *expected, Json *input) {
    printf("testing %s\n", expected);

    JsonSerializeFlags flags = JSON_FLAG_SPACES;
    char *res = json_serialize(input, flags);

    if (strcmp(res, expected) != 0) {
        printf("`%s` did not equal the expected `%s`\n", res, expected);
        assert(false);
    }

    free(res);
}

void test_primitives() {
    test("10", &(Json) {.type = JSON_TYPE_NUMBER, .inner.number = 10});
    test("true", &(Json) {.type = JSON_TYPE_BOOL, .inner.boolean = true});
    test("\"fooo\"", &(Json) {.type = JSON_TYPE_STRING, .inner.string = "fooo"});
}

int main() {
    test_primitives();
}
