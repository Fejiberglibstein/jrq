#ifndef _JSON_H
#define _JSON_H

#include "src/vector.h"
#include <stdbool.h>

typedef enum JsonType {
    JSON_TYPE_INVALID,
    JSON_TYPE_OBJECT,
    JSON_TYPE_LIST,
    JSON_TYPE_NULL,
    JSON_TYPE_NUMBER,
    JSON_TYPE_STRING,
    JSON_TYPE_BOOL,
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

bool json_equal(Json, Json);
Json json_copy(Json);
char *json_type(Json);

void json_list_append(Json *list, Json el);
Json json_list_sized(size_t i);

Json json_object_sized(size_t i);

Json json_append(Json, Json);

#endif // _JSON_H
