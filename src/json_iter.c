#include "json_iter.h"
#include "src/alloc.h"
#include "src/json.h"
#include <stdint.h>

#define NEXT(iter)                                                                                 \
    ({                                                                                             \
        IterOption __j = iter_next(iter);                                                          \
        if (__j.type == ITER_DONE) {                                                               \
            return __j;                                                                            \
        }                                                                                          \
        __j.some;                                                                                  \
    })

/// An IteratorFunc is the function that gets called every time `iter_next` is called.
///
/// From the point of rust iterators, this would be the `next` function in the impl block for the
/// iterator.
typedef IterOption (*IteratorFunc)(JsonIterator);

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

// clang-format off
IterOption iter_done() { return (IterOption) {.type = ITER_DONE}; }
IterOption iter_some(Json j) { return (IterOption) {.some = j, .type = ITER_SOME}; }
// clang-format on

/// Advances the iterator and returns the next value yielded.
///
/// Will return a `json_invalid()` when the iterator is over.
IterOption iter_next(JsonIterator iter) {
    if (iter == NULL) {
        return iter_done();
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

/// Used for iterators like next--Iterators that can capture state via a void*.
static void free_func_next_and_captures(JsonIterator i) {
    struct {
        struct JsonIterator i;
        JsonIterator next;
        void *closure_captures;
    } *iter = (typeof(iter))i;

    iter_free(iter->next);
    free(iter->closure_captures);
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
    static IterOption next_func_name(JsonIterator);                                                \
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
static IterOption list_iter_next(JsonIterator i) {
    ListIter *list_iter = (ListIter *)i;

    if (list_iter->index >= list_iter->data.inner.list.length) {
        return iter_done();
    }

    return iter_some(json_copy(list_iter->data.inner.list.data[list_iter->index++]));
}

/// Returns an iterator over the keys of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(KeyIter, iter_obj_keys, key_iter_next);
static IterOption key_iter_next(JsonIterator i) {
    KeyIter *key_iter = (KeyIter *)i;

    if (key_iter->index >= key_iter->data.inner.object.length) {
        return iter_done();
    }

    return iter_some(json_copy(key_iter->data.inner.object.data[key_iter->index++].key));
}

/// Returns an iterator over the values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(ValueIter, iter_obj_values, value_iter_next);
static IterOption value_iter_next(JsonIterator i) {
    ValueIter *value_iter = (ValueIter *)i;

    if (value_iter->index >= value_iter->data.inner.object.length) {
        return iter_done();
    }

    return iter_some(json_copy(value_iter->data.inner.object.data[value_iter->index++].value));
}

/// Returns an iterator over the keys and values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(KeyValueIter, iter_obj_key_value, key_value_iter_next);
static IterOption key_value_iter_next(JsonIterator i) {
    KeyValueIter *kv_iter = (KeyValueIter *)i;

    if (kv_iter->index >= kv_iter->data.inner.object.length) {
        return iter_done();
    }

    JsonObject obj = kv_iter->data.inner.object;
    Json ret = JSON_LIST(
        json_copy(obj.data[kv_iter->index].key), json_copy(obj.data[kv_iter->index].value)
    );
    kv_iter->index += 1;

    return iter_some(ret);
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

    /// Extra state to be passed into `func` when it's called
    void *closure_captures;

    /// Mapping function to apply to each element of the iterator
    MapFunc map_func;
} MapIter;

static IterOption map_iter_next(JsonIterator i) {
    MapIter *map_iter = (MapIter *)i;

    Json j = NEXT(map_iter->next);

    return iter_some(map_iter->map_func(j, map_iter->closure_captures));
}

/// An iterator that maps the values yielded by `iter` with `func`.
///
/// `captures` allow the func to have access to data outside the function, if
/// you want to emulate closure captures like in rust.
///
/// `captures` will be passed in as a parameter into `func` every time it is
/// called.
JsonIterator iter_map(JsonIterator iter, MapFunc func, void *captures, bool free_captures) {
    MapIter *i = jrq_malloc(sizeof(*i));

    *i = (MapIter) {
        .iter = { 
            .func = &map_iter_next,
            .free = free_captures ? &free_func_next_and_captures : &free_func_next,
        },
        .next = iter,
        .map_func = func,
        .closure_captures = captures,
    };

    return (JsonIterator)i;
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

    /// Extra state to be passed into `filter_func` when it's called
    void *closure_captures;

    /// Filtering function to apply to each element of the iterator
    FilterFunc filter_func;
} FilterIter;

static IterOption filter_iter_next(JsonIterator i) {
    FilterIter *filter_iter = (FilterIter *)i;

    for (;;) {
        Json j = NEXT(filter_iter->next);
        // if the filter function returns true, return it
        if (filter_iter->filter_func(j, filter_iter->closure_captures)) {
            return iter_some(j);
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
JsonIterator iter_filter(JsonIterator iter, FilterFunc func, void *captures, bool free_captures) {
    FilterIter *i = jrq_malloc(sizeof(*i));

    *i = (FilterIter) {
        .iter = { 
            .func = &filter_iter_next,
            .free = free_captures ? &free_func_next_and_captures : &free_func_next,
        },
        .next = iter,
        .filter_func = func,
        .closure_captures = captures,
    };

    return (JsonIterator)i;
}

/****************
 * EnumerateIter*
 ****************/

/// An iterator that creates a pair of [value, index] for each value yielded by
/// the iterator
typedef struct {
    struct JsonIterator iter;

    JsonIterator next;
    size_t index;

} EnumerateIter;

static IterOption enumerate_iter_next(JsonIterator i) {
    EnumerateIter *iter = (EnumerateIter *)i;

    Json j = NEXT(iter->next);

    return iter_some(JSON_LIST(j, json_number(iter->index++)));
}

/// An iterator that yields the current iteration index along with the next
/// value.
///
/// The iterator yields values in the form of [value, i]
JsonIterator iter_enumerate(JsonIterator iter) {
    EnumerateIter *i = jrq_malloc(sizeof(EnumerateIter));

    *i = (EnumerateIter) {
        .iter = { 
            .func = &enumerate_iter_next,
            .free = &free_func_next,
        },
        .next = iter,
        .index = 0,
    };

    return (JsonIterator)i;
}

Json iter_collect(JsonIterator i) {
    if (i == NULL) {
        return json_null();
    }
    Json list = json_list(); // TODO make iter_sized_hint

    IterOption opt;

    for (;;) {
        opt = iter_next(i);
        if (opt.type == ITER_SOME) {
            list = json_list_append(list, opt.some);
        } else {
            break;
        }
    }
    iter_free(i);

    return list;
}
