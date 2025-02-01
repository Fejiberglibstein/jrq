#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    int length;
    int capacity;
} String;

void string_grow(String *str, int amt) {
    if (str->capacity - str->length < amt) {
        do {
            str->capacity *= 2;
        } while (str->capacity - str->length < amt);

        str->data = realloc(str->data, str->capacity);
    }
}

void string_append(String *str, char *buf, int buf_len) {
    string_grow(str, buf_len);
    strcpy((str->data + str->length), buf);
    // - 1 for the null terminator
    str->length += buf_len - 1;
}
