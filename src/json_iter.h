#ifndef _JSON_ITER_H
#define _JSON_ITER_H

#include "src/json.h"

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

JsonIterator iter_map(JsonIterator i, Json (*f)(Json, void *), void *captures, bool free_captures);
JsonIterator iter_filter(
    JsonIterator iter,
    bool (*filter_func)(Json, void *),
    void *captures,
    bool free_captures
);
JsonIterator iter_enumerate(JsonIterator iter);
JsonIterator iter_zip(JsonIterator a, JsonIterator b);
Json iter_collect(JsonIterator iter);

JsonIterator iter_skip_while(
    JsonIterator iter,
    bool (*filter_func)(Json, void *),
    void *captures,
    bool free_captures
);
JsonIterator iter_take_while(
    JsonIterator iter,
    bool (*filter_func)(Json, void *),
    void *captures,
    bool free_captures
);

JsonIterator iter_skip(JsonIterator, int);
JsonIterator iter_take(JsonIterator, int);

#endif // _JSON_ITER_H
