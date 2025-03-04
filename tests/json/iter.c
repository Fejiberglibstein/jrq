#include "src/json.h"
#include "src/json_iter.h"
#include <assert.h>

Json mapper(Json, void *);

void basic_iter() {
    JsonIterator iter = iter_list(JSON_LIST(json_number(0), json_number(1), json_number(2)));
    assert(json_equal(iter_next(iter), json_number(0)));
    assert(json_equal(iter_next(iter), json_number(1)));
    assert(json_equal(iter_next(iter), json_number(2)));
    assert(json_equal(iter_next(iter), json_invalid()));
    assert(json_equal(iter_next(iter), json_invalid()));
    iter_free(iter);

    Json obj = JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2));
    iter = iter_obj_values(json_copy(obj));
    assert(json_equal(iter_next(iter), json_number(0)));
    assert(json_equal(iter_next(iter), json_number(1)));
    assert(json_equal(iter_next(iter), json_number(2)));
    assert(json_equal(iter_next(iter), json_invalid()));
    assert(json_equal(iter_next(iter), json_invalid()));
    iter_free(iter);

    iter = iter_obj_keys(obj);
    assert(json_equal(iter_next(iter), json_string_no_alloc("foo")));
    assert(json_equal(iter_next(iter), json_string_no_alloc("bar")));
    assert(json_equal(iter_next(iter), json_string_no_alloc("baz")));
    assert(json_equal(iter_next(iter), json_invalid()));
    assert(json_equal(iter_next(iter), json_invalid()));
    iter_free(iter);
}

void map_iter() {
    Json obj = JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2));
    JsonIterator iter = iter_obj_values(obj);
    iter = iter_map(iter, &mapper, NULL);
    assert(json_equal(iter_next(iter), json_number(0)));
    assert(json_equal(iter_next(iter), json_number(2)));
    assert(json_equal(iter_next(iter), json_number(4)));
    assert(json_equal(iter_next(iter), json_invalid()));
    assert(json_equal(iter_next(iter), json_invalid()));
    iter_free(iter);
}

int main() {
    basic_iter();
    map_iter();
}

Json mapper(Json j, void *_) {
    j.inner.number *= 2;
    return j;
}
