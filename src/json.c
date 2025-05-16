#include "src/json.h"
#include "src/strings.h"
#include "src/utils.h"
#include "src/vector.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON 0.00000001

typedef struct RefCnt {
    uint count;
} RefCnt;

uint refcnt_get(Json v) {
    switch (v.type) {
    case JSON_TYPE_OBJECT:
    case JSON_TYPE_LIST:
    case JSON_TYPE_STRING:
        return v.inner.ptr->count;
    default:
        return 1;
    }
}

void refcnt_inc(Json v) {
    switch (v.type) {
    case JSON_TYPE_OBJECT:
    case JSON_TYPE_LIST:
    case JSON_TYPE_STRING:
        v.inner.ptr->count++;
        break;
    default:
        break;
    }
}

// Returns true if there are no more references and it should be freed.
bool refcnt_dec(Json v) {
    switch (v.type) {
    case JSON_TYPE_OBJECT:
    case JSON_TYPE_LIST:
    case JSON_TYPE_STRING:
        return --v.inner.ptr->count == 0;

    default:
        return false;
    }
}

#define json_ptr_list(J) ((JsonListRef *)(J).inner.ptr)
#define json_ptr_object(J) ((JsonObjectRef *)(J).inner.ptr)
#define json_ptr_string(J) ((JsonStringRef *)(J).inner.ptr)

RefCnt *refcnt_init(size_t size) {
    RefCnt *r = malloc(size);
    memset(r, 0, size);
    r->count = 1;
    return r;
}

typedef struct {
    RefCnt ref;

    JsonList d;
} JsonListRef;

typedef struct {
    RefCnt ref;

    JsonObject d;
} JsonObjectRef;

typedef struct JsonStringRef {
    RefCnt ref;

    /// If the string is not a substring, this will be json_null
    ///
    /// When the string is a substring, this will point to the original string that this string is a
    /// substring of.
    Json borrowed_string;
    String d;
} JsonStringRef;

static char *json_list_type(JsonType type) {
    switch (type) {
    case JSON_TYPE_INVALID:
        return "list";
    case JSON_TYPE_OBJECT:
        return "list[object]";
    case JSON_TYPE_LIST:
        return "list[list]";
    case JSON_TYPE_NULL:
        return "list[<null>]";
    case JSON_TYPE_NUMBER:
        return "list[number]";
    case JSON_TYPE_STRING:
        return "list[string]";
    case JSON_TYPE_BOOL:
        return "list[bool]";
    default:
        return "list";
    }
}

char *json_type(Json j) {
    switch (j.type) {
    case JSON_TYPE_INVALID:
        return "<invalid>";
    case JSON_TYPE_OBJECT:
        return "object";
    case JSON_TYPE_LIST:
        return json_list_type(j.list_inner_type);
    case JSON_TYPE_NULL:
        return "<null>";
    case JSON_TYPE_NUMBER:
        return "number";
    case JSON_TYPE_STRING:
        return "string";
    case JSON_TYPE_BOOL:
        return "bool";
    default:
        unreachable("Cannot be an any");
    }
}

bool json_equal(Json j1, Json j2) {
    if (j1.type != j2.type) {
        return false;
    }

    bool result = true;

    switch (j1.type) {
    case JSON_TYPE_NUMBER:
        result = fabs(j1.inner.number - j2.inner.number) <= EPSILON;
        break;
    case JSON_TYPE_STRING:
        result = string_equal(*json_get_string(j1), *json_get_string(j2));
        break;
    case JSON_TYPE_BOOL:
        result = j1.inner.boolean == j2.inner.boolean;
        break;
    case JSON_TYPE_NULL:
        result = true;
        break;
    case JSON_TYPE_OBJECT:
        if (json_get_object(j1)->length != json_get_object(j2)->length) {
            result = false;
            break;
        }
        JsonObject *j1obj = json_get_object(j1);
        JsonObject *j2obj = json_get_object(j2);
        for (int i = 0; i < j1obj->length; i++) {
            if (json_equal(j1obj->data[i].key, j2obj->data[i].key) == false) {
                result = false;
                break;
            }
            if (json_equal(j1obj->data[i].value, j2obj->data[i].value) == false) {
                result = false;
                break;
            }
        }
        break;
    case JSON_TYPE_LIST:
        if (json_get_list(j1)->length != json_get_list(j2)->length) {
            result = false;
            break;
        }
        if (j1.list_inner_type != j2.list_inner_type) {
            result = false;
            break;
        }
        for (int i = 0; i < json_get_list(j1)->length; i++) {
            if (json_equal(json_list_get(j1, i), json_list_get(j2, i)) == false) {
                result = false;
                break;
            }
        }
        break;
    case JSON_TYPE_INVALID:
        result = true;
        break;
    case JSON_TYPE_ANY:
        unreachable("Json should not be an any");
    }

    // json_free(j1);
    // json_free(j2);

    return result;
}

Json json_copy(Json j) {
    refcnt_inc(j);
    return j;
}

Json json_clone(Json j) {
    Json new;
    switch (j.type) {
    case JSON_TYPE_INVALID:
    case JSON_TYPE_ANY:
    case JSON_TYPE_NUMBER:
    case JSON_TYPE_BOOL:
    case JSON_TYPE_NULL:
        return j;
    case JSON_TYPE_STRING:
        // TODO fix
        // TODO really important !!!!!!
        // TODO this should ensure that the proper substring is obtained from the original j.
        return json_string(string_get(json_get_string(j)));
    case JSON_TYPE_OBJECT:
        new = json_object_sized(json_get_object(j)->length);
        for (int i = 0; i < json_get_object(j)->length; i++) {
            JsonObjectPair pair = json_get_object(j)->data[i];
            json_object_set(new, json_clone(pair.key), json_clone(pair.value));
        }
        return new;
        break;
    case JSON_TYPE_LIST:
        new = json_list_sized(json_get_list(j)->length);
        for (int i = 0; i < json_get_list(j)->length; i++) {
            Json el = json_get_list(j)->data[i];
            json_list_append(new, json_clone(el));
        }
        return new;
        break;
    }

    // unreachable
    return json_invalid();
}

void json_free(Json j) {
    JsonObject *obj;
    JsonList *list;

    if (!refcnt_dec(j)) {
        return;
    }

    switch (j.type) {
    case JSON_TYPE_OBJECT:
        obj = json_get_object(j);
        for (int i = 0; i < obj->length; i++) {
            json_free(obj->data[i].value);
            json_free(obj->data[i].key);
        }
        free(obj->data);
        free(json_ptr_object(j));
        break;
    case JSON_TYPE_LIST:
        list = json_get_list(j);
        for (int i = 0; i < list->length; i++) {
            json_free(list->data[i]);
        }
        free(list->data);
        free(json_ptr_list(j));
        break;
    case JSON_TYPE_STRING:
        // If there is no capacity, then the string was not allocated as a json string
        if (json_ptr_string(j)->d.capacity == 0) {
            free(json_ptr_string(j));
            break;
        }
        if (!json_is_null(json_ptr_string(j)->borrowed_string)) {
            // If the string is a substring, we should free the string we're borrowing from
            json_free(json_ptr_string(j)->borrowed_string);
        } else {
            // if the string is not a substring, we can free the data like normal.
            free(json_ptr_string(j)->d.data);
        }
        free(json_ptr_string(j));
        break;
    case JSON_TYPE_NULL:
    case JSON_TYPE_NUMBER:
    case JSON_TYPE_BOOL:
    case JSON_TYPE_INVALID:
    case JSON_TYPE_ANY:
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
Json json_boolean(bool boolean) {
    return (Json) {.type = JSON_TYPE_BOOL, .inner.boolean = boolean};
}
Json json_null(void) {
    return (Json) {.type = JSON_TYPE_NULL};
}
Json json_invalid(void) {
    return (Json) {.type = JSON_TYPE_INVALID};
}

double json_get_number(Json j) {
    assert(j.type == JSON_TYPE_NUMBER);
    return j.inner.number;
}
bool json_get_bool(Json j) {
    assert(j.type == JSON_TYPE_BOOL);
    return j.inner.boolean;
}
String *json_get_string(Json j) {
    assert(j.type == JSON_TYPE_STRING);
    return &json_ptr_string(j)->d;
}
JsonList *json_get_list(Json j) {
    assert(j.type == JSON_TYPE_LIST);
    return &json_ptr_list(j)->d;
}
JsonObject *json_get_object(Json j) {
    assert(j.type == JSON_TYPE_OBJECT);
    return &json_ptr_object(j)->d;
}

// Json json_invalid_msg(char *format, ...) {
//     va_list args;
//     va_start(args, format);
//
//     char *msg;
//     vasprintf(&msg, format, args);
//
//     va_end(args);
//
//     return (Json) {.type = JSON_TYPE_INVALID, .inner.invalid = msg};
// }

Json json_list_sized(size_t i) {
    JsonList d = {0};
    vec_grow(d, i);

    JsonListRef *ref = (JsonListRef *)refcnt_init(sizeof(*ref));
    ref->d = d;

    return (Json) {
        .type = JSON_TYPE_LIST,
        .inner.ptr = (RefCnt *)ref,
        .list_inner_type = 0,
    };
}

Json json_list(void) {
    return json_list_sized(16);
}

static void list_set_inner_type(Json *j, JsonType inner) {
    if (j->list_inner_type == JSON_TYPE_INVALID) {
        j->list_inner_type = inner;
    } else if (j->list_inner_type != inner) {
        j->list_inner_type = JSON_TYPE_ANY;
    }
}

Json json_list_append(Json j, Json el) {
    assert(j.type == JSON_TYPE_LIST);

    list_set_inner_type(&j, el.type);
    vec_append(json_ptr_list(j)->d, el);
    return j;
}

Json json_list_get(Json j, uint index) {
    assert(j.type == JSON_TYPE_LIST);

    return json_ptr_list(j)->d.data[index];
}

JsonType json_list_get_inner_type(Json j) {
    assert(j.type == JSON_TYPE_LIST);

    return j.list_inner_type;
}

size_t json_list_length(Json j) {
    assert(j.type == JSON_TYPE_LIST);

    return json_ptr_list(j)->d.length;
}

// Don't use this, it's not implemented well
// Json json_list_set(Json j, uint index, Json val) {
//     assert(j.type == JSON_TYPE_LIST);
//
//     list_set_inner_type(&j, val.type);
//     json_ptr_list(j)->d.data[index] = val;
//
//     return j;
// }

Json json_object_sized(size_t i) {
    JsonObject d = {0};
    vec_grow(d, i);

    JsonObjectRef *ref = (JsonObjectRef *)refcnt_init(sizeof(*ref));
    ref->d = d;

    return (Json) {
        .type = JSON_TYPE_OBJECT,
        .inner.ptr = (RefCnt *)ref,
    };
}

Json json_object() {
    return json_object_sized(16);
}

Json json_object_set(Json j, Json key, Json value) {
    assert(j.type == JSON_TYPE_OBJECT);
    assert(key.type == JSON_TYPE_STRING);

    JsonObject *obj = json_get_object(j);
    for (int i = 0; i < obj->length; i++) {
        if (json_equal(obj->data[i].key, key)) {

            json_free(obj->data[i].value);
            json_free(key);

            obj->data[i].value = value;
            return j;
        }
    }
    vec_append(*json_get_object(j), (JsonObjectPair) {.key = key, .value = value});
    return j;
}

Json json_object_get(Json j, Json key) {
    assert(j.type == JSON_TYPE_OBJECT);
    assert(key.type == JSON_TYPE_STRING);

    JsonObject *obj = json_get_object(j);

    for (int i = 0; i < obj->length; i++) {
        if (json_equal(obj->data[i].key, key)) {
            return obj->data[i].value;
        }
    }

    return json_null();
}

size_t json_object_length(Json j) {
    assert(j.type == JSON_TYPE_OBJECT);

    return json_get_object(j)->length;
}

Json json_string(const char *str) {
    JsonStringRef *s = (JsonStringRef *)refcnt_init(sizeof(*s));

    s->d = string_from_chars_alloc((char *)str);
    s->borrowed_string = json_null();

    return (Json) {.type = JSON_TYPE_STRING, .inner.ptr = (RefCnt *)s};
}

Json json_string_from(String str) {
    JsonStringRef *s = (JsonStringRef *)refcnt_init(sizeof(*s));
    s->d = str;
    s->d.capacity = 0;
    return (Json) {.type = JSON_TYPE_STRING, .inner.ptr = (RefCnt *)s};
}

/// Create a json string out of the substring of another string.
///
/// This will NOT allocate a new string, instead it will borrow from the original string.
///
/// The original string will have another reference added to it, and when the substring is freed,
/// the original string will have a reference removed.
Json json_substring(Json str, size_t offset, size_t length) {
    assert(str.type == JSON_TYPE_STRING);

    JsonStringRef *s = (JsonStringRef *)refcnt_init(sizeof(*s));
    s->d = json_ptr_string(str)->d;
    s->d.data += offset;
    s->d.length = length + 1;
    s->d.capacity = 0;

    s->borrowed_string = str;
    refcnt_inc(str);
    return (Json) {.type = JSON_TYPE_STRING, .inner.ptr = (RefCnt *)s};
}

Json json_string_concat(Json j, Json str) {
    assert(j.type == JSON_TYPE_STRING);
    assert(str.type == JSON_TYPE_STRING);
    assert(json_get_string(j)->capacity != 0);

    string_append(json_get_string(j), *json_get_string(str));
    return j;
}

size_t json_string_length(Json j) {
    assert(j.type == JSON_TYPE_STRING);

    return json_ptr_string(j)->d.length;
}
