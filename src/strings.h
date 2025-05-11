#ifndef _STRINGS_H
#define _STRINGS_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct {
    char *data;
    uint length;
    uint capacity;
} String;

void string_grow(String *, uint);
bool string_equal(String, String);
char *string_get(String *);
void string_append(String *, String);
void string_printf(String *, const char *, ...);
String string_from_str(char *, uint);
String string_from_chars(char *);
String string_from_str_alloc(char *, uint);
String string_from_chars_alloc(char *);

#endif // _STRINGS_H
