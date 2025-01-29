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
        int Int;
        double Double;
        bool Bool;
        void *Null; /* this won't ever have meaningful data */
        char *String;
        struct Json *Struct;
        struct Json *List;
    } v;
    enum JsonType type;
} Json;

char *json_serialize(Json *json, char flags);
Json *json_deserialize(char *json);

#endif // JSON_H
