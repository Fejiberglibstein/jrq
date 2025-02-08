#ifndef _JSON_H
#define _JSON_H

#include <stdbool.h>
#define JSON_NO_COMPACT 0b00000001
#define JSON_COLOR 0b00000010

typedef enum JsonType {
    JSONTYPE_END_LIST,
    JSONTYPE_NUMBER,
    JSONTYPE_OBJECT,
    JSONTYPE_STRING,
    JSONTYPE_LIST,
    JSONTYPE_BOOL,
    JSONTYPE_NULL,
} JsonType;

typedef struct Json {
    char *field_name;
    union {
        double number;
        bool boolean;
        char *string;
        struct Json *object;
        struct Json *list;
    } inner;
    JsonType type;
} Json;

char *json_serialize(Json *json, char flags);
Json *json_deserialize(char *json);

#endif // _JSON_H
