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
        if (bytes < length) {
            break;
        }

        // Length = next capacity / 4
        length = capacity;
        capacity *= 2;

        char *tmp = jrq_realloc(start, capacity);
        if (tmp == NULL) {
            return NULL;
        }
        start = tmp;
        str = start + length;
    }

    return start;
}

int main(int argc, char *argv[]) {
    EvalResult res = eval(ast_parse(argv[1]).node, json_null());

    printf("%s\n", jrq_error_format(res.err, argv[1]));
}

int h_main(int argc, char **argv) {
    char *str = read_from_file(STDIN_FILENO);

    DeserializeResult res = json_deserialize(str);
    if (res.error != NULL) {
        printf("%s", res.error);
        exit(1);
    }
    free(str);

    JsonSerializeFlags flags = JSON_FLAG_TAB;
    if (isatty(STDOUT_FILENO)) {
        flags = JSON_FLAG_TAB | JSON_FLAG_COLORS;
    }

    char *out = json_serialize(&res.result, flags);
    printf("%s\n", out);

    free(out);
    return 0;
}
