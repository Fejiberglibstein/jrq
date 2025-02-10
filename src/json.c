#include "src/json.h"
#include "src/utils.h"
#include "src/vector.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON 0.00000001

struct JsonObjectInner {
    struct Json value;
    char *key;
};

bool json_equal(Json j1, Json j2) {
    if (j1.type != j2.type) {
        return false;
    }

    switch (j1.type) {
    case JSON_TYPE_NUMBER:
        return fabs(j1.inner.number - j2.inner.number) <= EPSILON;
    case JSON_TYPE_STRING:
        return strcmp(j1.inner.string, j2.inner.string) == 0;
    case JSON_TYPE_BOOL:
        return j1.inner.boolean == j2.inner.boolean;
    case JSON_TYPE_NULL:
        return true;
    case JSON_TYPE_OBJECT:
        if (j1.inner.object.length != j2.inner.object.length) {
            return false;
        }
        JsonObject j1obj = j1.inner.object;
        JsonObject j2obj = j2.inner.object;
        for (int i = 0; i < j1obj.length; i++) {
            if (strcmp(j1obj.data[i].key, j2obj.data[i].key) != 0) {
                return false;
            }
            if (json_equal(j1obj.data[i].value, j2obj.data[i].value) == false) {
                return false;
            }
        }
        return true;
    case JSON_TYPE_LIST:
        if (j1.inner.list.length != j2.inner.list.length) {
            return false;
        }
        for (int i = 0; i < j1.inner.list.length; i++) {
            if (json_equal(j1.inner.list.data[i], j2.inner.list.data[i]) == false) {
                return false;
            }
        }
        return true;
    case JSON_TYPE_INVALID:
        return true;
        break;
    }
}

Json json_copy(Json j) {
    Json new;
    switch (j.type) {
    case JSON_TYPE_INVALID:
    case JSON_TYPE_NUMBER:
    case JSON_TYPE_STRING:
    case JSON_TYPE_BOOL:
    case JSON_TYPE_NULL:
        return j;
    case JSON_TYPE_OBJECT:
        // TODO
        break;
    case JSON_TYPE_LIST:
        new = json_list_sized(j.inner.list.length);
        for (int i = 0; i < j.inner.list.length; i++) {
            Json el = json_copy(j.inner.list.data[i]);
            json_list_append(&new, el);
        }
        return new;
        break;
    }
}

Json json_list_sized(size_t i) {
    JsonIterator j = {0};
    vec_grow(j, i);
    return (Json) {
        .type = JSON_TYPE_LIST,
        .inner.list = j,
    };
}

Json json_list(void) {
    return (Json) {
        .type = JSON_TYPE_LIST,
        .inner.list = {0},
    };
}

void json_list_append(Json *list, Json el) {
    assert(list->type == JSON_TYPE_LIST);

    vec_append(list->inner.list, el);
}

Json json_object_sized(size_t i) {
    JsonObject j = {0};
    vec_grow(j, i);
    return (Json) {
        .type = JSON_TYPE_OBJECT,
        .inner.object = j,
    };
}

void json_object_set(Json *j, char *key, Json value) {
    assert(j->type == JSON_TYPE_OBJECT);

    vec_append(j->inner.object, (struct JsonObjectInner) {.key = key, .value = value});
}
