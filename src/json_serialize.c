#include "src/json.h"
#include "src/json_serde.h"
#include "src/utils.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define STRING_COLOR "\x1b[32m"
#define NUM_COLOR "\x1b[36m"
#define KEY_COLOR "\x1b[34;1m"
#define RESET_COLOR "\x1b[0m"
#define NULL_COLOR "\x1b[30;3m"
#define BOOL_COLOR "\x1b[31m"

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
    JsonList list = json->inner.list;
    if (list.length == 0) {
        string_append_str(s->inner, "[]");
        return;
    }
    string_append_str(s->inner, has_flag(s, JSON_FLAG_TAB) ? "[\n" : "[");

    for (int i = 0; i < list.length; i++) {

        tab(s, depth);

        serialize(s, &list.data[i], depth);

        if (i + 1 != list.length) {
            string_append_str(s->inner, has_flag(s, JSON_FLAG_SPACES) ? ", " : ",");
        }
        if (has_flag(s, JSON_FLAG_TAB)) {
            string_append_str(s->inner, "\n");
        }
    }

    tab(s, depth - 1);
    string_append_str(s->inner, "]");
}

static void serialize_object(Serializer *s, Json *json, int depth) {
    JsonObject fields = json->inner.object;
    if (fields.length == 0) {
        string_append_str(s->inner, "{}");
        return;
    }
    string_append_str(s->inner, has_flag(s, JSON_FLAG_TAB) ? "{\n" : "{");

    for (int i = 0; i < fields.length; i++) {

        tab(s, depth);

        // serialize object's key
        APPEND_COLOR(KEY_COLOR);
        string_append_str(s->inner, "\"");
        string_append_str(s->inner, fields.data[i].key);
        string_append_str(s->inner, "\"");
        APPEND_COLOR(RESET_COLOR);

        string_append_str(s->inner, has_flag(s, JSON_FLAG_SPACES) ? ": " : ":");

        serialize(s, &fields.data[i].value, depth);

        if (i + 1 != fields.length) {
            string_append_str(s->inner, has_flag(s, JSON_FLAG_SPACES) ? ", " : ",");
        }
        if (has_flag(s, JSON_FLAG_TAB)) {
            string_append_str(s->inner, "\n");
        }
    }

    tab(s, depth - 1);
    string_append_str(s->inner, "}");
}

void serialize(Serializer *s, Json *json, int depth) {
    switch (json->type) {
    case JSON_TYPE_INVALID:
        string_append_str(s->inner, "<invalid>");
        break;
    case JSON_TYPE_LIST:
        serialize_list(s, json, depth + 1);
        break;
    case JSON_TYPE_OBJECT:
        serialize_object(s, json, depth + 1);
        break;
    case JSON_TYPE_NUMBER:
        APPEND_COLOR(NUM_COLOR);

        int buf_len = snprintf(NULL, 0, "%g", json->inner.number) + 1;
        string_grow(s->inner, buf_len);
        snprintf(s->inner.data + s->inner.length, buf_len, "%g", json->inner.number);

        s->inner.length += buf_len - 1;

        APPEND_COLOR(RESET_COLOR);
        break;
    case JSON_TYPE_STRING:
        APPEND_COLOR(STRING_COLOR);
        string_append_str(s->inner, "\"");
        string_append_str(s->inner, json->inner.string);
        string_append_str(s->inner, "\"");
        APPEND_COLOR(RESET_COLOR);
        break;
    case JSON_TYPE_BOOL:
        APPEND_COLOR(BOOL_COLOR);
        string_append_str(s->inner, json->inner.boolean ? "true" : "false");
        APPEND_COLOR(RESET_COLOR);
        break;
    case JSON_TYPE_NULL:
        APPEND_COLOR(NULL_COLOR);
        string_append_str(s->inner, "null");
        APPEND_COLOR(RESET_COLOR);
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
