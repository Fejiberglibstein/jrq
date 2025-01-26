#include "./src/json.h"
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *read_from_file(int fd) {

    size_t capacity = 4;
    size_t length = 2;
    char *str = malloc(capacity);
    char *start = str;

    for (;;) {
        size_t bytes = read(fd, str, length);
        printf("%zu\n", bytes);
        if (bytes < 0) {
            return NULL;
        }
        if (bytes == 0) {
            break;
        }

        capacity *= 2;
        start = realloc(start, capacity);
        if (start == NULL) {
            return NULL;
        }

        str += length;
        length = capacity - length;
    }

    return start;
}

int main(int argc, char **argv) {

    char *str = read_from_file(STDIN_FILENO);
    printf("--%s\n", str);

    Json *json = json_deserialize(str);
    if (json == NULL) {
        return -1;
    }

    char flags = 0;
    if (isatty(STDOUT_FILENO)) {
        flags = JSON_COLOR | JSON_NO_COMPACT;
    }

    char *out = json_serialize(json, flags);
    printf("%s\n", out);

    free(out);
    free(json);
}
