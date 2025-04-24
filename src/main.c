#include "src/alloc.h"
#include "src/errors.h"
#include "src/eval.h"
#include "src/json.h"
#include "src/json_serde.h"
#include "src/parser.h"
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *read_from_file(FILE *file) {
    char *str = NULL;
    size_t size = 0;
    size_t bytes_read = getdelim(&str, &size, '\0', file);

    str = jrq_realloc(str, bytes_read + 1);
    str[bytes_read] = '\0';
    return str;
}

int main(int argc, char **argv) {
    char *str = read_from_file(stdin);

    DeserializeResult res = json_deserialize(str);
    if (res.type == RES_ERR) {
        char *err_string = jrq_error_format(res.err, str);
        printf("%s\n", err_string);
        free(err_string);

        free(str);
        exit(1);
    }
    free(str);

    Json result = res.result;

    if (argc > 1) {
        char *code = argv[1];
        ParseResult parse_res = ast_parse(code);
        if (parse_res.type == RES_ERR) {
            char *err_string = jrq_error_format(parse_res.err, code);
            printf("%s\n", err_string);
            json_free(result);
            free(err_string);
            exit(1);
        }

        ASTNode *ast = parse_res.node;

        EvalResult eval_res = eval(ast, result);
        json_free(result);

        if (eval_res.type == RES_ERR) {
            char *err_string = jrq_error_format(eval_res.err, code);
            printf("%s\n", err_string);
            free(err_string);
            exit(1);
        }

        // Change result to be the result of the evaluation.
        result = eval_res.json;
    }

    JsonSerializeFlags flags = JSON_FLAG_TAB;
    if (isatty(STDOUT_FILENO)) {
        flags = JSON_FLAG_TAB | JSON_FLAG_SPACES | JSON_FLAG_COLORS;
    }
    char *out = json_serialize(&result, flags);
    json_free(result);
    printf("%s\n", out);

    free(out);
    return 0;
}
