#ifndef _JSON_H
#define _JSON_H

#include "src/vector.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum JsonType {
    JSON_TYPE_INVALID,
    JSON_TYPE_OBJECT,
    JSON_TYPE_LIST,
    JSON_TYPE_NULL,
    JSON_TYPE_NUMBER,
    JSON_TYPE_STRING,
    JSON_TYPE_BOOL,
} JsonType;

typedef Vec(struct Json) JsonList;
typedef Vec(struct JsonObjectPair) JsonObject;

typedef struct Json {
    union {
        double number;
        bool boolean;
        char *string;
        char *invalid;
        JsonObject object;
        JsonList list;
    } inner;
    JsonType type;
} Json;

typedef struct JsonObjectPair {
    Json value;
    Json key;
} JsonObjectPair;

bool json_equal(Json, Json);
Json json_copy(Json);
void json_free(Json);

bool json_is_null(Json);
bool json_is_invalid(Json);

char *json_type(JsonType);

Json json_number(double f);
Json json_string(char *);
Json json_string_no_alloc(char *);
Json json_boolean(bool);
Json json_null(void);
Json json_list(void);
Json json_object(void);
Json json_invalid(void);
Json json_invalid_msg(char *, ...);

Json json_list_append(Json, Json);
Json json_list_sized(size_t);
Json json_list_get(Json, uint);

// clang-format off
#define JSON_LIST_1(e1) json_list_append(json_list(), e1)
#define JSON_LIST_2(e1, e2) json_list_append(JSON_LIST_1(e1), e2)
#define JSON_LIST_3(e1, e2, e3) json_list_append(JSON_LIST_2(e1, e2), e3)
#define JSON_LIST_4(e1, e2, e3, e4) json_list_append(JSON_LIST_3(e1, e2, e3), e4)
#define JSON_LIST_5(e1, e2, e3, e4, e5) json_list_append(JSON_LIST_4(e1, e2, e3, e4), e5)
#define JSON_LIST_6(e1, e2, e3, e4, e5, e6) json_list_append(JSON_LIST_5(e1, e2, e3, e4, e5), e6)
#define JSON_LIST_7(e1, e2, e3, e4, e5, e6, e7) json_list_append(JSON_LIST_6(e1, e2, e3, e4, e5, e6), e7)
#define JSON_LIST_8(e1, e2, e3, e4, e5, e6, e7, e8) json_list_append(JSON_LIST_7(e1, e2, e3, e4, e5, e6, e7), e8)
#define JSON_LIST_9(e1, e2, e3, e4, e5, e6, e7, e8, e9) json_list_append(JSON_LIST_8(e1, e2, e3, e4, e5, e6, e7, e8), e9)
#define JSON_LIST_10(e1, e2, e3, e4, e5, e6, e7, e8, e9, e10) json_list_append(JSON_LIST_9(e1, e2, e3, e4, e5, e6, e7, e8, e9), e10)
#define JSON_LIST_IDX(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, NAME, ...) NAME
// clang-format on
#define JSON_LIST(...)                                                                                                                                                    \
    JSON_LIST_IDX(__VA_ARGS__, JSON_LIST_10, JSON_LIST_9, JSON_LIST_8, JSON_LIST_7, JSON_LIST_6, JSON_LIST_5, JSON_LIST_4, JSON_LIST_3, JSON_LIST_2, JSON_LIST_1, dummy)( \
        __VA_ARGS__                                                                                                                                                       \
    )

Json json_object_sized(size_t);
Json json_object_set(Json j, Json key, Json value);
Json json_object_get(Json *j, char *key);

// clang-format off
#define JSON_OBJECT_1(k1, v1) json_object_set(json_object(), json_string(k1), v1)
#define JSON_OBJECT_2(k1, v1, k2, v2) json_object_set(JSON_OBJECT_1(k1, v1), json_string(k2), v2)
#define JSON_OBJECT_3(k1, v1, k2, v2, k3, v3) json_object_set(JSON_OBJECT_2(k1, v1, k2, v2), json_string(k3), v3)
#define JSON_OBJECT_4(k1, v1, k2, v2, k3, v3, k4, v4) json_object_set(JSON_OBJECT_3(k1, v1, k2, v2, k3, v3), json_string(k4), v4)
#define JSON_OBJECT_5(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5) json_object_set(JSON_OBJECT_4(k1, v1, k2, v2, k3, v3, k4, v4), json_string(k5), v5)
#define JSON_OBJECT_6(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6) json_object_set(JSON_OBJECT_5(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5), json_string(k6), v6)
#define JSON_OBJECT_7(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7, v7) json_object_set(JSON_OBJECT_6(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6), json_string(k7), v7)
#define JSON_OBJECT_8(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7, v7, k8, v8) json_object_set(JSON_OBJECT_7(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7, v7), json_string(k8), v8)
#define JSON_OBJECT_9(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7, v7, k8, v8, k9, v9) json_object_set(JSON_OBJECT_8(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7, v7, k8, v8), json_string(k9), v9)
#define JSON_OBJECT_10(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7, v7, k8, v8, k9, v9, k10, v10) json_object_set(JSON_OBJECT_9(k1, v1, k2, v2, k3, v3, k4, v4, k5, v5, k6, v6, k7, v7, k8, v8, k9, v9), json_string(k10), v10)
#define JSON_OBJECT_IDX(_k1, _v1, _k2, _v2, _k3, _v3, _k4, _v4, _k5, _v5, _k6, _v6, _k7, _v7, _k8, _v8, _k9, _v9, _k10, _v10, NAME, ...) NAME
// clang-format on
#define JSON_OBJECT(...)                                                                                                                                                                                                                 \
    JSON_OBJECT_IDX(__VA_ARGS__, JSON_OBJECT_10, _10, JSON_OBJECT_9, _9, JSON_OBJECT_8, _8, JSON_OBJECT_7, _7, JSON_OBJECT_6, _6, JSON_OBJECT_5, _5, JSON_OBJECT_4, _4, JSON_OBJECT_3, _3, JSON_OBJECT_2, _2, JSON_OBJECT_1, _1, dummy)( \
        __VA_ARGS__                                                                                                                                                                                                                      \
    )

#endif // _JSON_H
