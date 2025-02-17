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
typedef void (*FreeFunc)(JsonIterator);

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
    /// Function to call when `iter_next` is called on the iterator
    IteratorFunc func;

    // Function to be called when `iter_free` is called on the iterator
    FreeFunc free;
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

void iter_free(JsonIterator i) {
    i->free(i);
    free(i);
}

/*********
 * Utils *
 *********/

/// When using this method as the free function, you must have a `Json data`
/// field immediately after the JsonIterator in the iterator's struct definition
static void free_func_json(JsonIterator i) {
    struct {
        struct JsonIterator i;
        Json data;
    } *iter = (typeof(iter))i;

    json_free(iter->data);
}

/// When using this method as the free function, you must have a `JsonIterator next`
/// field immediately after the JsonIterator in the iterator's struct definition
static void free_func_next(JsonIterator i) {
    struct {
        struct JsonIterator i;
        JsonIterator next;
    } *iter = (typeof(iter))i;

    iter_free(iter->next);
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
    ListIter *i = jrq_malloc(sizeof(ListIter));

    *i = (ListIter){
        .iter = {
            .func = &list_iter_next,
            .free = &free_func_json,
        },
        .index = 0,
        .data = j,
    };

    return (JsonIterator)i;
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

    return key_iter->data.inner.object.data[key_iter->index++].key;
}

/// Returns an iterator over the keys of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JsonIterator iter_obj_keys(Json j) {
    KeyIter *i = jrq_malloc(sizeof(KeyIter));

    *i = (KeyIter) {
        .iter = {
            .func = &key_iter_next,
            .free = &free_func_json,
        },
        .index = 0,
        .data = j,
    };

    return (JsonIterator)i;
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
    ValueIter *i = jrq_malloc(sizeof(ValueIter));

    *i = (ValueIter) {
        .iter = {
            .func = &value_iter_next,
            .free = &free_func_json,
        },
        .index = 0,
        .data = j,
    };

    return (JsonIterator)i;
}

/*****************
 * KeyValue Iter *
 *****************/

/// An iterator over the values of an object
typedef struct {
    struct JsonIterator iter;
    /// The object being iterated over.
    Json data;
    /// The current index of the object.
    size_t index;
} KeyValueIter;

static Json key_value_iter_next(JsonIterator i) {
    KeyValueIter *kv_iter = (KeyValueIter *)i;

    if (kv_iter->index >= kv_iter->data.inner.object.length) {
        return json_invalid();
    }

    JsonObject obj = kv_iter->data.inner.object;
    Json ret = JSON_LIST(obj.data[kv_iter->index].key, obj.data[kv_iter->index].value);
    kv_iter->index += 1;

    return ret;
}

/// Returns an iterator over the keys and values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JsonIterator iter_obj_key_value(Json j) {
    KeyValueIter *i = jrq_malloc(sizeof(KeyValueIter));

    *i = (KeyValueIter) {
        .iter = {
            .func = &key_value_iter_next,
            .free = &free_func_json,
        },
        .index = 0,
        .data = j,
    };

    return (JsonIterator)i;
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

    /// The iterator we're mapping over
    JsonIterator next;

    /// Mapping function to apply to each element of the iterator
    MapFunc map_func;

    /// Extra state to be passed into `func` when it's called
    void *closure_captures;
} MapIter;

static Json map_iter_next(JsonIterator i) {
    MapIter *map_iter = (MapIter *)i;

    Json j = NEXT(map_iter->next);

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
    MapIter *map_iter = jrq_malloc(sizeof(MapIter));

    *map_iter = (MapIter) {
        .iter = { 
            .func = &map_iter_next,
            .free = &free_func_next,
        },
        .next = iter,
        .map_func = func,
        .closure_captures = captures,
    };

    return (JsonIterator)map_iter;
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

    /// The iterator we're mapping over
    JsonIterator next;

    /// Filtering function to apply to each element of the iterator
    FilterFunc filter_func;

    /// Extra state to be passed into `filter_func` when it's called
    void *closure_captures;
} FilterIter;

static Json filter_iter_next(JsonIterator i) {
    FilterIter *filter_iter = (FilterIter *)i;

    for (;;) {
        Json j = NEXT(filter_iter->next);
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
    FilterIter *filter_iter = jrq_malloc(sizeof(FilterIter));

    *filter_iter = (FilterIter) {
        .iter = { 
            .func = &filter_iter_next,
            .free = &free_func_next,
        },
        .next = iter,
        .filter_func = func,
        .closure_captures = captures,
    };

    return (JsonIterator)filter_iter;
}
