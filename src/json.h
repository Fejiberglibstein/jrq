#ifndef _JSON_H
#define _JSON_H

#include "src/parser.h"
#include <stdbool.h>

typedef enum {
    JSON_FLAG_TAB = 1,
    JSON_FLAG_COLORS = 2,
    JSON_FLAG_SPACES = 2,
} JsonSerializeFlags;

typedef enum JsonType {
    JSON_TYPE_NUMBER,
    JSON_TYPE_OBJECT,
    JSON_TYPE_STRING,
    JSON_TYPE_LIST,
    JSON_TYPE_BOOL,
    JSON_TYPE_NULL,
} JsonType;

typedef Vec(struct Json) JsonIterator;

typedef struct Json {
    char *field_name;
    union {
        double number;
        bool boolean;
        char *string;
        JsonIterator object;
        JsonIterator list;
    } inner;
    JsonType type;
} Json;

char *json_serialize(Json *json, JsonSerializeFlags flags);
Json *json_deserialize(char *json);

#endif // _JSON_H
