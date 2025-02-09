#ifndef _UTILS_H
#define _UTILS_H

#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Used to assert that the result of malloc/realloc/calloc did not return NULL.
//
// If they did return NULL, this function will exit the program
static inline void assert_ptr(void *p) {
    if (p == NULL) {
        printf("Out of memory :skull:\n");
        exit(1);
    }
}

typedef Vec(char) String;

#define string_grow(str, amt) vec_grow(str, amt)

#define string_append_str(str, buf)                                                                \
    string_grow(str, strlen(buf) + 1);                                                             \
    strcpy(((str).data + (str).length), buf);                                                      \
    (str).length += strlen(buf);

#define string_append(str, buf, len)                                                               \
    string_grow(str, len);                                                                         \
    strcpy(((str).data + (str).length), buf);                                                      \
    (str).length += buf_len - 1;

#endif // _UTILS_H
