#ifndef _JSON_H
#define _JSON_H

#include "src/vector.h"
#include <stdbool.h>

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


#endif // _JSON_H
