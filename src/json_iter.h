#ifndef _JSON_ITER_H
#define _JSON_ITER_H

#include "json.h"

typedef struct JsonIterator *JsonIterator;
typedef struct {
    Json some;
    enum {
        ITER_SOME,
        ITER_DONE,
    } type;
} IterOption;

IterOption iter_next(JsonIterator iter);
void iter_free(JsonIterator iter);

JsonIterator iter_obj_keys(Json j);
JsonIterator iter_obj_values(Json j);
JsonIterator iter_obj_key_value(Json j);

JsonIterator iter_list(Json j);

JsonIterator iter_map(JsonIterator iter, Json (*MapFunc)(Json, void *), void *captures);
JsonIterator iter_filter(JsonIterator iter, bool (*FilterFunc)(Json, void *), void *captures);

JsonIterator iter_enumerate(JsonIterator iter);
Json iter_collect(JsonIterator iter);

#endif // _JSON_ITER_H
