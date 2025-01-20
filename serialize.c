#include "./json.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

#define STRING_COLOR "\x1b[32m"
#define NUM_COLOR "\x1b[36m"
#define KEY_COLOR "\x1b[34;1m"
#define RESET_COLOR "\x1b[0m"
#define NULL_COLOR "\x1b[30;3m"
#define BOOL_COLOR "\x1b[31m"

#define APPEND_COLOR(color)                                                                        \
    if (colors) {                                                                                  \
        string_append(string, color, sizeof(color));                                               \
    }

typedef struct {
    char *data;
    int length;
    int capacity;
} StringBuffer;

void string_grow(StringBuffer *str, int amt) {
    if (str->capacity - str->length < amt) {
        do {
            str->capacity *= 2;
        } while (str->capacity - str->length < amt);

        str->data = realloc(str->data, str->capacity);
    }
}

void string_append(StringBuffer *str, char *buf, int buf_len) {
    string_grow(str, buf_len);
    strcpy((str->data + str->length), buf);
    // - 1 for the null terminator
    str->length += buf_len - 1;
}

void serialize(Json *json, StringBuffer *string, char *depth, bool colors, bool parsing_struct) {
    // Use the depth to calculate indentation level.
    //
    // depth is a string of just spaces: "    " represents one indentation
    // level. Having one string that's passed into functions instead of an int
    // for the depth minimizes heap allocations.
    //
    // If the depth is NULL, then we should not indent the json.
    // If the depth is "S", this is a special case where the indentation is 0.
    char *next_depth = NULL;
    int depth_len = 0;
    if (depth != NULL) {
        // If the depth has an S(kip) in it, we can skip adding indentation.
        //
        // S is only in depth at the shallowest indentation levels.
        if (depth[0] == 'S') {
            // If we have S, we can allow the next indentation level to be
            // filled
            next_depth = malloc(1);
            next_depth[0] = '\0';
            depth_len = 1;
        } else {
            depth_len = (((strlen(depth) / 4) + 1) * 4) + 1;
            next_depth = malloc(depth_len);
            memset(next_depth, ' ', depth_len);
            next_depth[depth_len] = '\0';
            string_append(string, next_depth, depth_len);
        }
    }

    // When parsing lists/structs, use a bool to determine what braces should be
    // used.
    bool is_struct = false;

    if (parsing_struct && json->field_name != NULL) {
        APPEND_COLOR(KEY_COLOR);

        string_append(string, "\"", 2);
        string_append(string, json->field_name, strlen(json->field_name) + 1);
        string_append(string, "\"", 2);

        APPEND_COLOR(RESET_COLOR);
        string_append(string, ": ", 3);
    }

    switch (json->type) {
    case JSONTYPE_NULL:
        APPEND_COLOR(NULL_COLOR);
        string_append(string, "null", sizeof("null"));
        APPEND_COLOR(RESET_COLOR);
        break;
    case JSONTYPE_BOOL:
        APPEND_COLOR(BOOL_COLOR);
        if (json->bool_type) {
            string_append(string, "true", sizeof("true"));
        } else {
            string_append(string, "false", sizeof("false"));
        }
        APPEND_COLOR(RESET_COLOR);
        break;
    case JSONTYPE_INT:
        APPEND_COLOR(NUM_COLOR);

        // Buffer for int and float types when converting to string
        char buf[32];
        int buf_len;

        buf_len = snprintf(NULL, 0, "%d", json->int_type) + 1;
        buf[buf_len] = '\0';
        snprintf(buf, buf_len, "%d", json->int_type);
        string_append(string, buf, buf_len);

        APPEND_COLOR(RESET_COLOR);
        break;

    case JSONTYPE_FLOAT:
        APPEND_COLOR(NUM_COLOR);

        buf_len = snprintf(NULL, 0, "%f", json->float_type) + 1;
        buf[buf_len] = '\0';
        snprintf(buf, buf_len, "%f", json->float_type);
        string_append(string, buf, buf_len);

        APPEND_COLOR(RESET_COLOR);
        break;

    case JSONTYPE_STRING:
        APPEND_COLOR(STRING_COLOR);

        string_append(string, "\"", 2);
        string_append(string, json->string_type, strlen(json->string_type) + 1);
        string_append(string, "\"", 2);

        APPEND_COLOR(RESET_COLOR);
        break;

    case JSONTYPE_STRUCT:
        is_struct = true;
    case JSONTYPE_LIST:
        // Parsing a struct and list are effectively the same thing, so we just
        // use a simple switch for which parenthesis type to use
        //
        // '{}' for structs, and `[]` for lists
#define L_PAREN(...) is_struct ? "{" __VA_ARGS__ : "[" __VA_ARGS__
#define R_PAREN(...) is_struct ? "}" __VA_ARGS__ : "]" __VA_ARGS__
        bool skip_depth = json->list_type[0].type == JSONTYPE_END_LIST;
        Json *list = json->list_type;

        if (depth == NULL || skip_depth) {
            string_append(string, L_PAREN(), 2);
        } else {
            string_append(string, L_PAREN("\n"), 3);
        }

        for (int i = 0; list[i].type != JSONTYPE_END_LIST; i++) {
            serialize(&(list[i]), string, next_depth, colors, is_struct);
            if (list[i + 1].type != JSONTYPE_END_LIST) {
                string_append(string, ", ", 3);
            }
            if (depth != NULL && !skip_depth) {
                string_append(string, "\n", 2);
            }
        }

        if (next_depth != NULL && !skip_depth) {
            string_append(string, next_depth, depth_len);
        }
        string_append(string, R_PAREN(), 2);
        break;
#undef L_PAREN
#undef R_PAREN
    case JSONTYPE_END_LIST:
        // unreachable
        break;
    }

    if (next_depth != NULL) {
        free(next_depth);
    }
}

char *json_serialize(Json *json, char flags) {
    StringBuffer str = (StringBuffer
    ) {.data = malloc(INITIAL_CAPACITY), .length = 0, .capacity = INITIAL_CAPACITY};

    int depth = (flags & JSON_NO_COMPACT) ? 0 : -1;
    bool color = (flags & JSON_COLOR);

    serialize(json, &str, depth ? NULL : "S", color, false);
    return str.data;
}
