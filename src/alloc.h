#ifndef _ALLOC_H
#define _ALLOC_H

#include <stdlib.h>

void *jrq_malloc(size_t);
void *jrq_calloc(size_t, size_t);
void *jrq_realloc(void *, size_t);
void *jrq_strdup(char *);
char *jrq_strndup(char *, size_t);

#endif // _ALLOC_H
