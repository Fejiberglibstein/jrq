#include "src/json.h"
#include "src/json_iter.h"
#include "src/json_serde.h"
#include <assert.h>
#include <stdio.h>

Json mapper(Json, void *);

#define LIST(s...) s, (sizeof(s) / sizeof(*(s)))

void test_iter(JsonIterator iter, Json results) {

    for (int i = 0; i < json_list_length(results); i++) {
        IterOption res = iter_next(iter);
        assert(json_equal(res.some, json_list_get(results, i)));
        json_free(res.some);
    }
    assert(iter_next(iter).type == ITER_DONE);
    assert(iter_next(iter).type == ITER_DONE);

    iter_free(iter);
    json_free(results);
}

void basic_iter() {
    test_iter(
        iter_list(JSON_LIST(json_number(0), json_number(1), json_number(2))),
        JSON_LIST(json_number(0), json_number(1), json_number(2))
    );
    test_iter(
        iter_obj_keys(
            JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2))
        ),
        JSON_LIST(json_string("foo"), json_string("bar"), json_string("baz"))
    );
}

void map_iter() {
    Json obj = JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2));
    JsonIterator iter = iter_obj_values(obj);
    iter = iter_map(iter, &mapper, NULL, false);

    test_iter(iter, JSON_LIST(json_number(0), json_number(2), json_number(4)));
}

void enumerate_iter() {
    JsonIterator iter = iter_enumerate(iter_obj_key_value(
        JSON_OBJECT("blhe", json_null(), "grhas", json_boolean(false), "fhytr", json_number(6))
    ));
    test_iter(
        iter,
        JSON_LIST(
            JSON_LIST(json_number(0), JSON_LIST(json_string("blhe"), json_null())),
            JSON_LIST(json_number(1), JSON_LIST(json_string("grhas"), json_boolean(false))),
            JSON_LIST(json_number(2), JSON_LIST(json_string("fhytr"), json_number(6)))
        )
    );
}

void split_iter() {
    test_iter(
        iter_split(json_string("i am going to cry"), json_string(" ")),
        JSON_LIST(
            json_string("i"),
            json_string("am"),
            json_string("going"),
            json_string("to"),
            json_string("cry")
        )
    );
}

int main() {
    basic_iter();
    map_iter();
    enumerate_iter();
    split_iter();
}

Json mapper(Json j, void *_) {
    return json_number(json_get_number(j) * 2);
}
