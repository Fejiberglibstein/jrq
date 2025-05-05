#include "src/eval/private.h"
#include "src/json.h"
#include "src/json_serde.h"
#include "src/utils.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define STRING_COLOR "\x1b[32m"
#define NUM_COLOR "\x1b[36m"
#define KEY_COLOR "\x1b[34;1m"
#define RESET_COLOR "\x1b[0m"
#define NULL_COLOR "\x1b[90;3m"
#define BOOL_COLOR "\x1b[31m"
#define SYMBOL_COLOR "\x1b[97m"

#define APPEND_COLOR(color)                                                                        \
    if (has_flag(s, JSON_FLAG_COLORS)) {                                                           \
        string_append_str(s->inner, color);                                                        \
    }

typedef struct {
    String inner;
    JsonSerializeFlags flags;
} Serializer;

static void serialize(Serializer *s, Json *json, int depth);
static void serialize_object(Serializer *s, Json *json, int depth);
static void serialize_list(Serializer *s, Json *json, int depth);

static bool has_flag(Serializer *s, JsonSerializeFlags flag) {
    return (s->flags & flag) ? true : false;
}

static void tab(Serializer *s, int depth) {
    if (has_flag(s, JSON_FLAG_TAB)) {
        for (int d = 0; d < depth; d++) {
            string_append_str(s->inner, "    ");
        }
    }
}

static void serialize_list(Serializer *s, Json *json, int depth) {
    JsonList *list = json_get_list(*json);
    if (list->length == 0) {
        APPEND_COLOR(SYMBOL_COLOR);
        string_append_str(s->inner, "[]");
        APPEND_COLOR(RESET_COLOR);
        return;
    }
    APPEND_COLOR(SYMBOL_COLOR);
    string_append_str(s->inner, has_flag(s, JSON_FLAG_TAB) ? "[\n" : "[");
    APPEND_COLOR(RESET_COLOR);

    for (int i = 0; i < list->length; i++) {

        tab(s, depth);

        serialize(s, &list->data[i], depth);

        if (i + 1 != list->length) {
            APPEND_COLOR(SYMBOL_COLOR);
            string_append_str(s->inner, has_flag(s, JSON_FLAG_SPACES) ? ", " : ",");
            APPEND_COLOR(RESET_COLOR);
        }
        if (has_flag(s, JSON_FLAG_TAB)) {
            string_append_str(s->inner, "\n");
        }
    }

    tab(s, depth - 1);
    APPEND_COLOR(SYMBOL_COLOR);
    string_append_str(s->inner, "]");
    APPEND_COLOR(RESET_COLOR);
}

static void serialize_object(Serializer *s, Json *json, int depth) {
    JsonObject *fields = json_get_object(*json);
    if (fields->length == 0) {
        APPEND_COLOR(SYMBOL_COLOR);
        string_append_str(s->inner, "{}");
        APPEND_COLOR(RESET_COLOR);
        return;
    }
    APPEND_COLOR(SYMBOL_COLOR);
    string_append_str(s->inner, has_flag(s, JSON_FLAG_TAB) ? "{\n" : "{");
    APPEND_COLOR(RESET_COLOR);

    for (int i = 0; i < fields->length; i++) {

        tab(s, depth);

        // serialize object's key
        APPEND_COLOR(KEY_COLOR);
        string_append_str(s->inner, "\"");
        string_append_str(s->inner, json_get_string(fields->data[i].key));
        string_append_str(s->inner, "\"");
        APPEND_COLOR(RESET_COLOR);

        APPEND_COLOR(SYMBOL_COLOR);
        string_append_str(s->inner, has_flag(s, JSON_FLAG_SPACES) ? ": " : ":");
        APPEND_COLOR(RESET_COLOR);

        serialize(s, &fields->data[i].value, depth);

        if (i + 1 != fields->length) {
            APPEND_COLOR(SYMBOL_COLOR);
            string_append_str(s->inner, has_flag(s, JSON_FLAG_SPACES) ? ", " : ",");
            APPEND_COLOR(RESET_COLOR);
        }
        if (has_flag(s, JSON_FLAG_TAB)) {
            string_append_str(s->inner, "\n");
        }
    }

    tab(s, depth - 1);
    APPEND_COLOR(SYMBOL_COLOR);
    string_append_str(s->inner, "}");
    APPEND_COLOR(RESET_COLOR);
}

void serialize(Serializer *s, Json *json, int depth) {
    switch (json->type) {
    case JSON_TYPE_INVALID:
        // if (json->inner.invalid != NULL) {
        //     string_append_str(s->inner, "<invalid: ");
        //     string_append_str(s->inner, json->inner.invalid);
        //     string_append_str(s->inner, ">");
        // } else {
        string_append_str(s->inner, "<invalid>");
        // }
        break;
    case JSON_TYPE_LIST:
        serialize_list(s, json, depth + 1);
        break;
    case JSON_TYPE_OBJECT:
        serialize_object(s, json, depth + 1);
        break;
    case JSON_TYPE_NUMBER:
        APPEND_COLOR(NUM_COLOR);

        int buf_len = snprintf(NULL, 0, "%g", json_get_number(*json)) + 1;
        string_grow(s->inner, buf_len);
        snprintf(s->inner.data + s->inner.length, buf_len, "%g", json_get_number(*json));

        s->inner.length += buf_len - 1;

        APPEND_COLOR(RESET_COLOR);
        break;
    case JSON_TYPE_STRING:
        APPEND_COLOR(STRING_COLOR);
        string_append_str(s->inner, "\"");
        string_append_str(s->inner, json_get_string(*json));
        string_append_str(s->inner, "\"");
        APPEND_COLOR(RESET_COLOR);
        break;
    case JSON_TYPE_BOOL:
        APPEND_COLOR(BOOL_COLOR);
        string_append_str(s->inner, json_get_bool(*json) ? "true" : "false");
        APPEND_COLOR(RESET_COLOR);
        break;
    case JSON_TYPE_NULL:
        APPEND_COLOR(NULL_COLOR);
        string_append_str(s->inner, "null");
        APPEND_COLOR(RESET_COLOR);
        break;
    case JSON_TYPE_ANY:
        unreachable("should not be any here");
        break;
    }
}

char *json_serialize(Json *json, JsonSerializeFlags flags) {
    Serializer *s = &(Serializer) {
        .inner = (String) {0},
        .flags = flags,
    };

    serialize(s, json, 0);
    return s->inner.data;
}
