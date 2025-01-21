#include "./src/json.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    char *str;

    if (argc < 2) {
        return -1;
    }
    str = argv[1];

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
