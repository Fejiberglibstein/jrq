#include "strings.h"
#include "src/alloc.h"
#include "vector.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void string_grow(String *str, uint amt) {
    vec_grow(*str, amt);
}

bool string_equal(String *a, String *b) {
    if (a->length != b->length) {
        return false;
    }
    return strncmp(string_get(a), string_get(b), a->length) == 0;
}

char *string_get(String *str) {
    return str->data;
}

void string_append(String *a, String b) {
    string_grow(a, b.length + 1);
    strncpy((a->data + a->length), string_get(&b), b.length);
    a->length += b.length;
    string_get(a)[a->length] = '\0';
}

void string_printf(String *s, const char *fmt, ...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    int buf_len = vsnprintf(NULL, 0, fmt, args1) + 1;
    string_grow(s, buf_len + 1);
    vsnprintf(&string_get(s)[s->length], buf_len, fmt, args2);
    s->length += buf_len - 1;

    string_get(s)[s->length] = '\0';

    va_end(args1);
    va_end(args2);
}

String string_from_str(char *str, uint len) {
    return (String) {
        .data = str,
        .length = len,
        .capacity = 0,
    };
}

String string_from_chars(char *str) {
    return string_from_str(str, strlen(str));
}

String string_from_str_alloc(char *str, uint len) {
    return (String) {
        .length = len,
        .data = jrq_strndup(str, len),
        .capacity = len,
    };
}

String string_from_chars_alloc(char *str) {
    return string_from_str_alloc(str, strlen(str));
}
