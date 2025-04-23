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

char *read_from_file(int fd) {

    size_t capacity = 4;
    char *str = jrq_malloc(capacity);
    char *start = str;

    size_t length = 0;

    for (;;) {
        // If length is 0, we should read the initial value of capacity bytes,
        // otherwise read the actual length
        size_t bytes_to_read = (length) ? length : capacity;
        size_t bytes = read(fd, str, bytes_to_read);
        if (bytes < 0) {
            return NULL;
        }
        if (bytes == 0) {
            break;
        }

        // Length = next capacity / 4
        length = capacity;
        capacity *= 2;

        start = jrq_realloc(start, capacity);
        str = start + length;
    }

    return start;
}

int main(int argc, char **argv) {
    char *str = read_from_file(STDIN_FILENO);

    DeserializeResult res = json_deserialize(str);
    if (res.type == RES_ERR) {
        printf("%s\n", jrq_error_format(res.err, str));

        free(str);
        exit(1);
    }
    free(str);

    Json result = res.result;

    if (argc > 1) {
        char *code = argv[1];
        ParseResult parse_res = ast_parse(code);
        if (parse_res.type == RES_ERR) {
            printf("%s\n", jrq_error_format(parse_res.err, code));
            exit(1);
        }

        ASTNode *ast = parse_res.node;

        EvalResult eval_res = eval(ast, result);
        free(ast);

        if (eval_res.type == RES_ERR) {
            printf("%s\n", jrq_error_format(eval_res.err, code));
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
    printf("%s\n", out);

    free(out);
    return 0;
}
