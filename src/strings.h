#ifndef _STRINGS_H
#define _STRINGS_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    size_t offset;
} String;

void string_grow(String *, uint);
bool string_equal(String *, String *);
char *string_get(String *);
void string_append(String *, String);
void string_printf(String *, const char *, ...);
String string_from_str(char *, uint);
String string_from_chars(char *);
String string_from_str_alloc(char *, uint);
String string_from_chars_alloc(char *);

#endif // _STRINGS_H
