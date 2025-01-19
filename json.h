#ifndef JSON_H
#define JSON_H

#include <stdint.h>
#define JSON_NO_COMPACT 0b00000001
#define JSON_COLOR 0b00000010

enum Type : char {
    TYPE_END_LIST,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRUCT,
    TYPE_STRING,
    TYPE_LIST,
};

typedef struct Json {
    char *field_name;
    union {
        int int_type;
        float float_type;
        struct Json *struct_type;
        char *string_type;
        struct Json *list_type;
    };
    enum Type type;
} Json;

char *json_serialize(Json *json, uint8_t flags);

#endif // JSON_H
