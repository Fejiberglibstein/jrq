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

/// A FreeFunc is the function that gets called when `iter_free` is called.
///
/// This function should free any inner state the iterator implementation may
/// have.
///
/// You do NOT need to free the iterator itself inside `FreeFunc`s, freeing the
/// iterator is handled by `iter_free`
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

    /// Function to be called when `iter_free` is called on the iterator
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
    if (i->free != NULL) {
        i->free(i);
    }
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

#define JSON_DATA_ITERATOR(struct_name, create_func_name, next_func_name)                          \
    JsonIterator create_func_name(Json);                                                           \
                                                                                                   \
    typedef struct {                                                                               \
        struct JsonIterator iter;                                                                  \
        /* the json data being iterating over */                                                   \
        Json data;                                                                                 \
        /* the current index of the data we're iterating over */                                   \
        size_t index;                                                                              \
    } struct_name;                                                                                 \
                                                                                                   \
    static Json next_func_name(JsonIterator);                                                      \
                                                                                                   \
    JsonIterator create_func_name(Json j) {                                                        \
        struct_name *i = jrq_malloc(sizeof(struct_name));                                          \
                                                                                                   \
        *i = (struct_name) {                                                                       \
            .iter = {.func = &next_func_name, .free = &free_func_json},                            \
            .index = 0,                                                                            \
            .data = j,                                                                             \
        };                                                                                         \
                                                                                                   \
        return (JsonIterator)i;                                                                    \
    }

/// Returns an iterator over all of the values in a list
///
/// If `j` is not a list, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(ListIter, iter_list, list_iter_next);
static Json list_iter_next(JsonIterator i) {
    ListIter *list_iter = (ListIter *)i;

    if (list_iter->index >= list_iter->data.inner.list.length) {
        return json_invalid();
    }

    return list_iter->data.inner.list.data[list_iter->index++];
}

/// Returns an iterator over the keys of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(KeyIter, iter_obj_keys, key_iter_next);
static Json key_iter_next(JsonIterator i) {
    KeyIter *key_iter = (KeyIter *)i;

    if (key_iter->index >= key_iter->data.inner.object.length) {
        return json_invalid();
    }

    return key_iter->data.inner.object.data[key_iter->index++].key;
}

/// Returns an iterator over the values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(ValueIter, iter_obj_values, value_iter_next);
static Json value_iter_next(JsonIterator i) {
    ValueIter *value_iter = (ValueIter *)i;

    if (value_iter->index >= value_iter->data.inner.object.length) {
        return json_invalid();
    }

    return value_iter->data.inner.object.data[value_iter->index++].value;
}

/// Returns an iterator over the keys and values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(KeyValueIter, iter_obj_key_value, key_value_iter_next);
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
