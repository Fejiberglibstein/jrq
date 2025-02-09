#ifndef _JSON_SERDE_H
#define _JSON_SERDE_H

#include "src/json.h"

typedef enum {
    JSON_FLAG_TAB = 1,
    JSON_FLAG_COLORS = 2,
    JSON_FLAG_SPACES = 4,
} JsonSerializeFlags;

typedef struct {
    char *error;
    Json *result;
} DeserializeResult;

char *json_serialize(Json *json, JsonSerializeFlags flags);
DeserializeResult json_deserialize(char *json);

#endif // _JSON_SERDE_H
