#ifndef JSON_H
#define JSON_H

#include <stdint.h>
#include <stdbool.h>
#define JSON_NO_COMPACT 0b00000001
#define JSON_COLOR 0b00000010

enum Type : char {
    TYPE_END_LIST,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRUCT,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_BOOL,
    TYPE_NULL,
};

typedef struct Json {
    char *field_name;
    union {
        int int_type;
        float float_type;
        bool bool_type;
        void *null_type; /* this won't ever have meaningful data */
        char *string_type;
        struct Json *struct_type;
        struct Json *list_type;
    };
    enum Type type;
} Json;

char *json_serialize(Json *json, uint8_t flags);

#endif // JSON_H
