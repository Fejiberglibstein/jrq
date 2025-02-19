#include "src/eval.h"
#include "src/json.h"
#include "src/json_serde.h"
#include "src/parser.h"
#include <assert.h>
#include <stdio.h>

bool test_eval(char *expr, Json input, Json expected) {
    printf("Testing `%s`\n", expr);
    ASTNode *node = ast_parse(expr).node;

    Json result = eval(node, input);

    bool r = json_equal(result, expected);

    if (r == false) {
        char *result_str = json_serialize(&result, 0);
        char *expected_str = json_serialize(&expected, 0);

        printf("`%s` does not equal expected `%s`\n", result_str, expected_str);

        free(result_str);
        free(expected_str);
    }

    json_free(input);
    json_free(expected);
    json_free(result);

    if (node != NULL) {
        ast_free(node);
    }

    return r;
}

void simple_eval() {
    assert(test_eval("", json_number(10), json_number(10)));
    assert(test_eval("10 + 10", json_null(), json_number(20)));
}

int main() {
    simple_eval();
}
