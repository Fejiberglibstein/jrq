#include "strings.h"
#include "src/alloc.h"
#include "vector.h"
#include <stdbool.h>
#include <stdio.h>

void string_grow(String *str, uint amt) {
    vec_grow(*str, amt);
}

char *string_get(String *str) {
    return str->data + str->offset;
}

void string_append(String *a, String b) {
    string_grow(a, b.length);
    strncpy((a->data + a->length - 1), string_get(&b), b.length);
    a->length += b.length - 1;
}

String string_from_str(char *str, uint len) {
    return (String) {
        .data = str,
        .length = len,
        .offset = 0,
        .capacity = 0,
    };
}

String string_from_chars(char *str) {
    return string_from_str(str, strlen(str) + 1);
}

String string_from_str_alloc(char *str, uint len) {
    return (String) {
        .length = len,
        .data = jrq_strndup(str, len),
        .offset = 0,
        .capacity = len,
    };
}

String string_from_chars_alloc(char *str) {
    return string_from_str_alloc(str, strlen(str) + 1);
}
