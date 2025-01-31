#ifndef _UTILS_H
#define _UTILS_H

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
        printf("Out of memory :skull:");
        exit(1);
    }
}

typedef struct {
    char *data;
    int length;
    int capacity;
} String;

void string_grow(String *str, int amt);

void string_append(String *str, char *buf, int buf_len);

#endif // _UTILS_H
