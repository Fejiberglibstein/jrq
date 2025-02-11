#include "json_iter.h"
#include "src/alloc.h"
#include "src/json.h"
#include <stdint.h>

#define NEXT(iter)                                                                                 \
    ({                                                                                             \
        Json __j = iter_next(iter);                                                                \
        if (json_is_invalid(__j)) {                                                                \
            return __j;                                                                            \
        }                                                                                          \
        __j;                                                                                       \
    })

/// An IteratorFunc is the function that gets called every time `iter_next` is called.
///
/// From the point of rust iterators, this would be the `next` function in the impl block for the
/// iterator.
typedef Json (*IteratorFunc)(JsonIterator *);

/// Base "class" for an iterator.
///
/// Each specific iterator implementation will "extend" this by having a JsonIterator as the first
/// field in the struct. After the JsonIterator struct, the specific Iterator implementation can
/// have any data it requires.
typedef struct JsonIterator {
    /// Function to call when `json_next` is called on the iterator
    IteratorFunc func;

    /// The next iterator to call in the chain, if NULL then nothing else needs
    /// to be evaluated.
    ///
    /// Acts sort of like a linked list, operating on a pipeline of data
    JsonIterator *next;
} JsonIterator;

/// Advances the iterator and returns the next value yielded.
///
/// Will return a `json_invalid()` when the iterator is over.
Json iter_next(JsonIterator *iter) {
    if (iter != NULL) {
        return json_invalid();
    }
    return iter->func(iter->next);
}

/***********
 * KeyIter *
 ***********/

typedef struct {
    JsonIterator iter;
    Json data;
    size_t index;
} KeyIter;

static Json iter_keys_next(JsonIterator *i) {
    KeyIter *key_iter = (KeyIter *)i;

    if (key_iter->index >= key_iter->data.inner.object.length) {
        return json_invalid();
    }

    char *key = key_iter->data.inner.object.data[key_iter->index++].key;
    char *str = jrq_strdup(key);

    return json_string(str);
}

JsonIterator *iter_keys(Json j) {
    JsonIterator *iter = jrq_malloc(sizeof(KeyIter));

    iter->func = &iter_keys_next;
    iter->next = NULL;
    ((KeyIter *)iter)->data = j;
    ((KeyIter *)iter)->index = 0;

    return iter;
}

/**************
 * Value Iter *
 **************/

typedef struct {
    JsonIterator iter;
    Json data;
    size_t index;
} ValueIter;

static Json iter_values_next(JsonIterator *i) {
    ValueIter *value_iter = (ValueIter *)i;

    if (value_iter->index >= value_iter->data.inner.object.length) {
        return json_invalid();
    }

    return value_iter->data.inner.object.data[value_iter->index++].value;
}

/// Returns an iterator over the values of a json object.
///
/// If the passed in json is not an object, the iterator will immediately return `json_invalid()`
JsonIterator *iter_values(Json j) {
    JsonIterator *iter = jrq_malloc(sizeof(ValueIter));

    iter->func = &iter_values_next;
    iter->next = NULL;
    ((ValueIter *)iter)->data = j;
    ((ValueIter *)iter)->index = 0;

    return iter;
}

typedef Json (*MapFunc)(Json, void *);
typedef struct {
    JsonIterator iter;
    MapFunc func;
    void *closure_captures;
} MapIter;

static Json iter_map_next(JsonIterator *i) {
    Json j = NEXT(i->next);

    MapIter *map_iter = (MapIter *)i;

    return map_iter->func(j, map_iter->closure_captures);
}

JsonIterator *iter_map(JsonIterator *i, MapFunc func, void *captures) {
    JsonIterator *iter = jrq_malloc(sizeof(MapIter));

    iter->next = i;
    iter->func = &iter_map_next;

    ((MapIter *)iter)->func = func;
    ((MapIter *)iter)->closure_captures = captures;

    return iter;
}
