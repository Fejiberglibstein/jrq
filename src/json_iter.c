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
typedef Json (*IteratorFunc)(JsonIterator);

/// Base "class" for an iterator.
///
/// Each specific iterator implementation will "extend" this by having a JsonIterator as the first
/// field in the struct. After the JsonIterator struct, the specific Iterator implementation can
/// have any data it requires.
///
/// NOTE:
///     All iterator implementations MUST use `struct JsonIterator` rather than `JsonIterator` as
///     their first field. `JsonIterator` is a pointer, not the iterator itself.
struct JsonIterator {
    /// Function to call when `json_next` is called on the iterator
    IteratorFunc func;

    /// The next iterator to call in the chain, if NULL then nothing else needs
    /// to be evaluated.
    ///
    /// Acts sort of like a linked list, operating on a pipeline of data
    JsonIterator next_iter;
};

/// Advances the iterator and returns the next value yielded.
///
/// Will return a `json_invalid()` when the iterator is over.
Json iter_next(JsonIterator iter) {
    if (iter == NULL) {
        return json_invalid();
    }
    return iter->func(iter);
}

/************
 * ListIter *
 ************/

/// Iterator over a list
typedef struct {
    struct JsonIterator iter;
    /// the list being iterating over
    Json data;
    /// the current index of the list
    size_t index;
} ListIter;

static Json list_iter_next(JsonIterator i) {
    ListIter *list_iter = (ListIter *)i;

    if (list_iter->index >= list_iter->data.inner.object.length) {
        return json_invalid();
    }

    return list_iter->data.inner.list.data[list_iter->index++];
}

/// Returns an iterator over all of the values in a list
///
/// If `j` is not a list, the iterator will yield `json_invalid()`.
JsonIterator iter_list(Json j) {
    JsonIterator iter = jrq_malloc(sizeof(ListIter));

    iter->func = &list_iter_next;
    iter->next_iter = NULL;

    ((ListIter *)iter)->index = 0;
    ((ListIter *)iter)->data = j;

    return iter;
}

/***********
 * KeyIter *
 ***********/

/// An iterator over the keys of an object
typedef struct {
    struct JsonIterator iter;
    /// The object being iterated over.
    Json data;
    /// The current index of the object.
    size_t index;
} KeyIter;

static Json key_iter_next(JsonIterator i) {
    KeyIter *key_iter = (KeyIter *)i;

    if (key_iter->index >= key_iter->data.inner.object.length) {
        return json_invalid();
    }

    char *key = key_iter->data.inner.object.data[key_iter->index++].key;
    char *str = jrq_strdup(key);

    return json_string(str);
}

/// Returns an iterator over the keys of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JsonIterator iter_obj_keys(Json j) {
    JsonIterator iter = jrq_malloc(sizeof(KeyIter));

    iter->func = &key_iter_next;
    iter->next_iter = NULL;
    ((KeyIter *)iter)->data = j;
    ((KeyIter *)iter)->index = 0;

    return iter;
}

/**************
 * Value Iter *
 **************/

/// An iterator over the values of an object
typedef struct {
    struct JsonIterator iter;
    /// The object being iterated over.
    Json data;
    /// The current index of the object.
    size_t index;
} ValueIter;

static Json value_iter_next(JsonIterator i) {
    ValueIter *value_iter = (ValueIter *)i;

    if (value_iter->index >= value_iter->data.inner.object.length) {
        return json_invalid();
    }

    return value_iter->data.inner.object.data[value_iter->index++].value;
}

/// Returns an iterator over the values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JsonIterator iter_obj_values(Json j) {
    JsonIterator iter = jrq_malloc(sizeof(ValueIter));

    iter->func = &value_iter_next;
    iter->next_iter = NULL;
    ((ValueIter *)iter)->data = j;
    ((ValueIter *)iter)->index = 0;

    return iter;
}

/***********
 * MapIter *
 ***********/

/// Function that maps some value to another value.
///
/// Also allows for extra state to be captured from passing in an extra
/// void * parameter
typedef Json (*MapFunc)(Json, void *);
/// An iterator that maps elements of `iter` by applying `func`
typedef struct {
    struct JsonIterator iter;
    /// Mapping function to apply to each element of the iterator
    MapFunc map_func;
    /// Extra state to be passed into `func` when it's called
    void *closure_captures;
} MapIter;

static Json map_iter_next(JsonIterator i) {
    Json j = NEXT(i->next_iter);

    MapIter *map_iter = (MapIter *)i;

    return map_iter->map_func(j, map_iter->closure_captures);
}

/// An iterator that maps the values yielded by `iter` with `func`.
///
/// `captures` allow the func to have access to data outside the function, if
/// you want to emulate closure captures like in rust.
///
/// `captures` will be passed in as a parameter into `func` every time it is
/// called.
JsonIterator iter_map(JsonIterator iter, MapFunc func, void *captures) {
    JsonIterator map_iter = jrq_malloc(sizeof(MapIter));

    map_iter->next_iter = iter;
    map_iter->func = &map_iter_next;

    ((MapIter *)map_iter)->map_func = func;
    ((MapIter *)map_iter)->closure_captures = captures;

    return map_iter;
}

/**************
 * FilterIter *
 **************/

/// Function that filters Json by returning true or false based on the
/// parameters passsed in
///
/// Also allows for extra state to be captured from passing in an extra
/// void * parameter
typedef bool (*FilterFunc)(Json, void *);
/// An iterator that filters elements of `iter` by skipping values if `filter_func`
/// returns false
typedef struct {
    struct JsonIterator iter;
    /// Filtering function to apply to each element of the iterator
    FilterFunc filter_func;
    /// Extra state to be passed into `filter_func` when it's called
    void *closure_captures;
} FilterIter;

static Json filter_iter_next(JsonIterator i) {
    FilterIter *filter_iter = (FilterIter *)i;

    for (;;) {
        Json j = NEXT(i->next_iter);
        // if the filter function returns true, return it
        if (filter_iter->filter_func(j, filter_iter->closure_captures)) {
            return j;
        }
    }
}

/// An iterator that maps the values yielded by `iter` with `func`.
///
/// `captures` allow the func to have access to data outside the function, if
/// you want to emulate closure captures like in rust.
///
/// `captures` will be passed in as a parameter into `func` every time it is
/// called.
JsonIterator iter_filter(JsonIterator iter, FilterFunc func, void *captures) {
    JsonIterator filter_iter = jrq_malloc(sizeof(FilterFunc));

    filter_iter->next_iter = iter;
    filter_iter->func = &filter_iter_next;

    ((FilterIter *)filter_iter)->filter_func = func;
    ((FilterIter *)filter_iter)->closure_captures = captures;

    return filter_iter;
}
