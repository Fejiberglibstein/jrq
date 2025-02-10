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

typedef Vec(struct Json) JsonList;
typedef Vec(struct JsonObjectPair) JsonObject;

typedef struct Json {
    union {
        double number;
        bool boolean;
        char *string;
        JsonObject object;
        JsonList list;
    } inner;
    JsonType type;
} Json;

typedef struct JsonObjectPair {
    Json value;
    char *key;
} JsonObjectPair;

bool json_equal(Json, Json);
Json json_copy(Json);
void json_free(Json);

char *json_type(Json);

void json_list_append(Json *, Json);
Json json_list_sized(size_t);

Json json_object_sized(size_t);
void json_object_set(Json *, char *, Json);

#endif // _JSON_H
