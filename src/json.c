#include "src/json.h"
#include "src/alloc.h"
#include "src/vector.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON 0.00000001

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
            if (json_equal(j1obj.data[i].key, j2obj.data[i].key) == false) {
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

    // unreachable
    return true;
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
        new = json_object_sized(j.inner.list.length);
        for (int i = 0; i < j.inner.object.length; i++) {
            JsonObjectPair pair = j.inner.object.data[i];

            Json key = json_copy(pair.key);

            Json value = json_copy(pair.value);
            new = json_object_set(new, key, value);
        }
        return new;
        break;
    case JSON_TYPE_LIST:
        new = json_list_sized(j.inner.list.length);
        for (int i = 0; i < j.inner.list.length; i++) {
            Json el = json_copy(j.inner.list.data[i]);
            new = json_list_append(new, el);
        }
        return new;
        break;
    }

    // unreachable
    return json_invalid();
}

void json_free(Json j) {
    JsonObject obj;
    JsonList list;
    switch (j.type) {
    case JSON_TYPE_OBJECT:
        obj = j.inner.object;
        for (int i = 0; i < obj.length; i++) {
            json_free(obj.data[i].value);
            json_free(obj.data[i].key);
        }
        free(obj.data);
        break;
    case JSON_TYPE_LIST:
        list = j.inner.list;
        for (int i = 0; i < list.length; i++) {
            json_free(list.data[i]);
        }
        free(list.data);
        break;
    case JSON_TYPE_NULL:
    case JSON_TYPE_INVALID:
    case JSON_TYPE_NUMBER:
    case JSON_TYPE_BOOL:
        break;
    case JSON_TYPE_STRING:
        free(j.inner.string);
        break;
    }
}

bool json_is_null(Json j) {
    return j.type == JSON_TYPE_NULL;
}

bool json_is_invalid(Json j) {
    return j.type == JSON_TYPE_INVALID;
}

Json json_number(double f) {
    return (Json) {.type = JSON_TYPE_NUMBER, .inner.number = f};
}

Json json_string(char *str) {
    return (Json) {.type = JSON_TYPE_STRING, .inner.string = jrq_strdup(str)};
}

Json json_boolean(bool boolean) {
    return (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = boolean};
}

Json json_null(void) {
    return (Json) {.type = JSON_TYPE_NULL};
}

Json json_invalid(void) {
    return (Json) {.type = JSON_TYPE_INVALID};
}

Json json_list_sized(size_t i) {
    JsonList j = {0};
    vec_grow(j, i);
    return (Json) {
        .type = JSON_TYPE_LIST,
        .inner.list = j,
    };
}

Json json_list(void) {
    return json_list_sized(16);
}

Json json_list_append(Json list, Json el) {
    assert(list.type == JSON_TYPE_LIST);

    vec_append(list.inner.list, el);
    return list;
}

Json json_object_sized(size_t i) {
    JsonObject j = {0};
    vec_grow(j, i);
    return (Json) {
        .type = JSON_TYPE_OBJECT,
        .inner.object = j,
    };
}

Json json_object() {
    return json_object_sized(16);
}

Json json_object_set(Json j, Json key, Json value) {
    assert(j.type == JSON_TYPE_OBJECT);
    assert(key.type == JSON_TYPE_STRING);

    JsonObject obj = j.inner.object;
    for (int i = 0; i < obj.length; i++) {
        if (json_equal(obj.data[i].key, key)) {
            json_free(obj.data[i].value);
            obj.data[i].value = value;
            return j;
        }
    }
    vec_append(j.inner.object, (JsonObjectPair) {.key = key, .value = value});
    return j;
}

Json json_object_get(Json *j, char *key) {
    assert(j->type == JSON_TYPE_OBJECT);
    JsonObject obj = j->inner.object;

    for (int i = 0; i < obj.length; i++) {
        if (strcmp(key, obj.data[i].key.inner.string) == 0) {
            return obj.data[i].value;
        }
    }

    return json_null();
}
