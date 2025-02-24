#include "src/json.h"
#include "src/json_iter.h"
#include <assert.h>

Json mapper(Json, void *);

void basic_iter() {
    JsonIterator iter = iter_list(JSON_LIST(json_number(0), json_number(1), json_number(2)));
    assert(json_equal(iter_next(iter).some, json_number(0)));
    assert(json_equal(iter_next(iter).some, json_number(1)));
    assert(json_equal(iter_next(iter).some, json_number(2)));
    assert(iter_next(iter).type == ITER_DONE);
    assert(iter_next(iter).type == ITER_DONE);
    iter_free(iter);

    Json obj = JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2));
    iter = iter_obj_values(json_copy(obj));
    assert(json_equal(iter_next(iter).some, json_number(0)));
    assert(json_equal(iter_next(iter).some, json_number(1)));
    assert(json_equal(iter_next(iter).some, json_number(2)));
    assert(iter_next(iter).type == ITER_DONE);
    assert(iter_next(iter).type == ITER_DONE);
    iter_free(iter);

    iter = iter_obj_keys(obj);
    assert(json_equal(iter_next(iter).some, json_string_no_alloc("foo")));
    assert(json_equal(iter_next(iter).some, json_string_no_alloc("bar")));
    assert(json_equal(iter_next(iter).some, json_string_no_alloc("baz")));
    assert(iter_next(iter).type == ITER_DONE);
    assert(iter_next(iter).type == ITER_DONE);
    iter_free(iter);
}

void map_iter() {
    Json obj = JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2));
    JsonIterator iter = iter_obj_values(obj);
    iter = iter_map(iter, &mapper, NULL);
    assert(json_equal(iter_next(iter).some, json_number(0)));
    assert(json_equal(iter_next(iter).some, json_number(2)));
    assert(json_equal(iter_next(iter).some, json_number(4)));
    assert(iter_next(iter).type == ITER_DONE);
    assert(iter_next(iter).type == ITER_DONE);
    iter_free(iter);
}

void enumerate_iter() {
    Json obj
        = JSON_OBJECT("blhe", json_null(), "grhas", json_boolean(false), "fhytr", json_number(6));

    JsonIterator iter = iter_enumerate(iter_obj_key_value(obj));
    Json results[] = {
        JSON_LIST(JSON_LIST(json_string("blhe"), json_null()), json_number(0)),
        JSON_LIST(JSON_LIST(json_string("grhas"), json_boolean(false)), json_number(1)),
        JSON_LIST(JSON_LIST(json_string("fhytr"), json_number(6)), json_number(2)),
    };

    for (int i = 0; i < 3; i++) {
        IterOption res = iter_next(iter);
        assert(json_equal(res.some, results[i]));
        json_free(res.some);
        json_free(results[i]);
    }
    assert(iter_next(iter).type == ITER_DONE);
    assert(iter_next(iter).type == ITER_DONE);

    iter_free(iter);
}

int main() {
    basic_iter();
    map_iter();
    enumerate_iter();
}

Json mapper(Json j, void *_) {
    j.inner.number *= 2;
    return j;
}
