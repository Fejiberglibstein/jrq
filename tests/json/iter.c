#include "src/json.h"
#include "src/json_iter.h"
#include "src/json_serde.h"
#include <assert.h>
#include <stdio.h>

Json mapper(Json, void *);

#define LIST(s...) s, (sizeof(s) / sizeof(*(s)))

void test_iter(JsonIterator iter, Json *results, int length) {
    Json j = (Json) {
        .type = JSON_TYPE_LIST,
        .inner.list = (JsonList) {
            .length = length,
            .data = results,
        },
    };

    char *str = json_serialize(&j, 0);
    printf("Testing `%s`\n", str);
    free(str);

    for (int i = 0; i < length; i++) {
        IterOption res = iter_next(iter);
        assert(json_equal(res.some, results[i]));
        json_free(res.some);
        json_free(results[i]);
    }
    assert(iter_next(iter).type == ITER_DONE);
    assert(iter_next(iter).type == ITER_DONE);
    iter_free(iter);
}

void basic_iter() {
    test_iter(
        iter_list(JSON_LIST(json_number(0), json_number(1), json_number(2))),
        LIST((Json[]) {json_number(0), json_number(1), json_number(2)})
    );
    test_iter(
        iter_obj_keys(
            JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2))
        ),
        LIST((Json[]) {json_string("foo"), json_string("bar"), json_string("baz")})
    );
}

void map_iter() {
    Json obj = JSON_OBJECT("foo", json_number(0), "bar", json_number(1), "baz", json_number(2));
    JsonIterator iter = iter_obj_values(obj);
    iter = iter_map(iter, &mapper, NULL, false);

    test_iter(iter, LIST((Json[]) {json_number(0), json_number(2), json_number(4)}));
}

void enumerate_iter() {
    JsonIterator iter = iter_enumerate(iter_obj_key_value(
        JSON_OBJECT("blhe", json_null(), "grhas", json_boolean(false), "fhytr", json_number(6))
    ));
    test_iter(
        iter,
        LIST((Json[]) {
            JSON_LIST(JSON_LIST(json_string("blhe"), json_null()), json_number(0)),
            JSON_LIST(JSON_LIST(json_string("grhas"), json_boolean(false)), json_number(1)),
            JSON_LIST(JSON_LIST(json_string("fhytr"), json_number(6)), json_number(2)),
        })
    );
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
