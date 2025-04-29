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
static void free_func_json(JsonIterator _i) {
    struct {
        struct JsonIterator parent;
        Json data;
    } *i = (typeof(i))_i;

    json_free(i->data);
}

/// When using this method as the free function, you must have a `JsonIterator next`
/// field immediately after the JsonIterator in the iterator's struct definition
static void free_func_next(JsonIterator _i) {
    struct {
        struct JsonIterator parent;
        JsonIterator iter;
    } *i = (typeof(i))_i;

    iter_free(i->iter);
}

/// Used for iterators like next--Iterators that can capture state via a void*.
static void free_func_next_and_captures(JsonIterator _i) {
    struct {
        struct JsonIterator parent;
        JsonIterator iter;
        void *closure_captures;
    } *i = (typeof(i))_i;

    iter_free(i->iter);
    free(i->closure_captures);
}

#define JSON_DATA_ITERATOR(STRUCT_NAME, CREATE_FUNC_NAME, NEXT_FUNC_NAME)                          \
    typedef struct {                                                                               \
        struct JsonIterator parent;                                                                \
        /* the json data being iterating over */                                                   \
        Json data;                                                                                 \
        /* the current index of the data we're iterating over */                                   \
        size_t index;                                                                              \
    } STRUCT_NAME;                                                                                 \
                                                                                                   \
    static IterOption NEXT_FUNC_NAME(JsonIterator);                                                \
                                                                                                   \
    JsonIterator CREATE_FUNC_NAME(Json j) {                                                        \
        STRUCT_NAME *i = jrq_malloc(sizeof(*i));                                                   \
                                                                                                   \
        *i = (STRUCT_NAME) {                                                                       \
            .parent = {.func = &NEXT_FUNC_NAME, .free = &free_func_json},                          \
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
static IterOption list_iter_next(JsonIterator _i) {
    ListIter *i = (ListIter *)_i;

    if (i->index >= json_get_list(i->data)->length) {
        return iter_done();
    }

    return iter_some(json_copy(json_list_get(i->data, i->index++)));
}

/// Returns an iterator over the keys of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(KeyIter, iter_obj_keys, key_iter_next);
static IterOption key_iter_next(JsonIterator _i) {
    KeyIter *i = (KeyIter *)_i;

    if (i->index >= json_get_object(i->data)->length) {
        return iter_done();
    }

    return iter_some(json_copy(json_get_object(i->data)->data[i->index++].key));
}

/// Returns an iterator over the values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(ValueIter, iter_obj_values, value_iter_next);
static IterOption value_iter_next(JsonIterator _i) {
    ValueIter *i = (ValueIter *)_i;

    if (i->index >= json_get_object(i->data)->length) {
        return iter_done();
    }

    return iter_some(json_copy(json_get_object(i->data)->data[i->index++].value));
}

/// Returns an iterator over the keys and values of a json object.
///
/// If `j` is not an object, the iterator will yield `json_invalid()`.
JSON_DATA_ITERATOR(KeyValueIter, iter_obj_key_value, key_value_iter_next);
static IterOption key_value_iter_next(JsonIterator _i) {
    KeyValueIter *i = (KeyValueIter *)_i;

    if (i->index >= json_get_object(i->data)->length) {
        return iter_done();
    }

    JsonObject *obj = json_get_object(i->data);
    Json ret = JSON_LIST(json_copy(obj->data[i->index].key), json_copy(obj->data[i->index].value));
    i->index += 1;

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
    struct JsonIterator parent;

    /// The iterator we're mapping over
    JsonIterator iter;

    /// Extra state to be passed into `func` when it's called
    void *closure_captures;

    /// Mapping function to apply to each element of the iterator
    MapFunc map_func;
} MapIter;

static IterOption map_iter_next(JsonIterator _i) {
    MapIter *i = (MapIter *)_i;

    Json j = NEXT(i->iter);

    return iter_some(i->map_func(j, i->closure_captures));
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
        .parent = { 
            .func = &map_iter_next,
            .free = free_captures ? &free_func_next_and_captures : &free_func_next,
        },
        .iter = iter,
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
    struct JsonIterator parent;

    /// The iterator we're mapping over
    JsonIterator iter;

    /// Extra state to be passed into `filter_func` when it's called
    void *closure_captures;

    /// Filtering function to apply to each element of the iterator
    FilterFunc filter_func;
} FilterIter;

static IterOption filter_iter_next(JsonIterator _i) {
    FilterIter *i = (FilterIter *)_i;

    for (;;) {
        Json j = NEXT(i->iter);
        // if the filter function returns true, return it
        if (i->filter_func(j, i->closure_captures)) {
            return iter_some(j);
        } else {
            json_free(j);
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
        .parent = { 
            .func = &filter_iter_next,
            .free = free_captures ? &free_func_next_and_captures : &free_func_next,
        },
        .iter = iter,
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
    struct JsonIterator parent;

    JsonIterator iter;
    size_t index;

} EnumerateIter;

static IterOption enumerate_iter_next(JsonIterator _i) {
    EnumerateIter *i = (EnumerateIter *)_i;

    Json j = NEXT(i->iter);

    return iter_some(JSON_LIST(json_number(i->index++), j));
}

/// An iterator that yields the current iteration index along with the next
/// value.
///
/// The iterator yields values in the form of [value, i]
JsonIterator iter_enumerate(JsonIterator iter) {
    EnumerateIter *i = jrq_malloc(sizeof(*i));

    *i = (EnumerateIter) {
        .parent = { 
            .func = &enumerate_iter_next,
            .free = &free_func_next,
        },
        .iter = iter,
        .index = 0,
    };

    return (JsonIterator)i;
}

/// Collect the iterator into a list
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

/***********
 * ZipIter *
 ***********/

/// An iterator that iterates over two other iterators simultaneously
///
/// The iterator yields values in the form [a, b]
typedef struct {
    struct JsonIterator parent;

    JsonIterator a;
    JsonIterator b;
} ZipIter;

IterOption zip_iter_next(JsonIterator _i) {
    ZipIter *i = (ZipIter *)_i;

    Json a = NEXT(i->a);
    Json b = NEXT(i->b);

    return iter_some(JSON_LIST(a, b));
}

JsonIterator iter_zip(JsonIterator a, JsonIterator b) {
    ZipIter *i = jrq_malloc(sizeof(*i));

    *i = (ZipIter) {
        .parent = { 
            .func = &zip_iter_next,
            .free = &free_func_next,
        },
        .a = a,
        .b = b,
    };

    return (JsonIterator)i;
}
