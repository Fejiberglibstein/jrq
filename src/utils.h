#ifndef _UTILS_H
#define _UTILS_H

#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define unreachable(str) assert(false && "unreachable: " str)

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

typedef Vec(char) String;

#define string_grow(str, amt) vec_grow(str, amt)

#define string_append_str(str, buf)                                                                \
    string_grow(str, strlen(buf) + 1);                                                             \
    strcpy(((str).data + (str).length), buf);                                                      \
    (str).length += strlen(buf);

#define string_append(str, buf, len)                                                               \
    string_grow(str, len);                                                                         \
    strncpy(((str).data + (str).length), buf, len);                                                \
    (str).length += (len) - 1;

#endif // _UTILS_H
