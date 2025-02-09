#include "src/json.h"
#include "src/utils.h"
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#define STRING_COLOR "\x1b[32m"
#define NUM_COLOR "\x1b[36m"
#define KEY_COLOR "\x1b[34;1m"
#define RESET_COLOR "\x1b[0m"
#define NULL_COLOR "\x1b[30;3m"
#define BOOL_COLOR "\x1b[31m"

#define APPEND_COLOR(color)                                                                        \
    if (s->flags & JSON_SER_COLOR) {                                                               \
        string_append(string, color, sizeof(color));                                               \
    }

typedef struct {
    String inner;
    JsonSerializeFlags flags;
} Serializer;

static bool has_flag(Serializer *s, JsonSerializeFlags flag) {
    return (s->flags & flag) ? true : false;
}

void serialize(Serializer *s, Json *json, int depth) {
    switch (json->type) {
    case JSON_TYPE_END_LIST:
    case JSON_TYPE_NUMBER:
    case JSON_TYPE_OBJECT:
    case JSON_TYPE_STRING:
    case JSON_TYPE_LIST:
    case JSON_TYPE_BOOL:
    case JSON_TYPE_NULL:
        break;
    }
}

char *json_serialize(Json *json, JsonSerializeFlags flags) {
    Serializer *s = &(Serializer) {
        .inner = (String) {0},
        .flags = flags,
    };

    serialize(s, json, flags);
    return s->inner.data;
}
