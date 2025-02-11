#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void out_of_memory() {
    fprintf(stderr, "Out of memory :skull:");
    abort();
}

void *jrq_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        out_of_memory();
    }
    return p;
}

void *jrq_calloc(size_t amt, size_t size) {
    void *p = calloc(size, amt);
    if (!p) {
        out_of_memory();
    }
    return p;
}

void *jrq_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p) {
        out_of_memory();
    }
    return p;
}

void *jrq_strdup(char *str) {
    void *p = strdup(str);
    if (!p) {
        out_of_memory();
    }
    return p;
}
