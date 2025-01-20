#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#define JSON_NO_COMPACT 0b00000001
#define JSON_COLOR 0b00000010

enum JsonType : char {
    JSONTYPE_END_LIST,
    JSONTYPE_INT,
    JSONTYPE_FLOAT,
    JSONTYPE_STRUCT,
    JSONTYPE_STRING,
    JSONTYPE_LIST,
    JSONTYPE_BOOL,
    JSONTYPE_NULL,
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
    enum JsonType type;
} Json;

char *json_serialize(Json *json, char flags);
Json *json_deserialize(char *json);

#endif // JSON_H
