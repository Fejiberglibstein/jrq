#include "./json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 4

typedef struct {
    int *data;
    int length;
    int capacity;
} IntBuffer;

enum keyword {
    KEYWORD_TRUE,
    KEYWORD_FALSE,
    KEYWORD_NULL,
};

typedef struct {
    char *end;
    enum JsonType type;
    union {
        char *String;
        int Int;
        float Float;
        enum keyword keyword;
    } v;
} ParsedValue;

void buf_grow(IntBuffer *buf, int amt) {
    if (buf->capacity - buf->length < amt) {
        buf->capacity *= 2;

        buf->data = realloc(buf->data, buf->capacity);
    }
}

void buf_append(IntBuffer *buf, int val) {
    buf_grow(buf, 1);
    buf->data[buf->length++] = val;
}

char *validate_string(char *json);
char *validate_number(char *json);
char *validate_keyword(char *json);
char *validate_list(char *json, IntBuffer *int_buf);
char *validate_struct(char *json, IntBuffer *int_buf);
char *validate_json(char *json, IntBuffer *int_buf);

ParsedValue parse_string(char *str);
ParsedValue parse_number(char *str);
ParsedValue parse_keyword(char *str);
ParsedValue parse_list(char *str, Json *arena, IntBuffer *buf, int buf_idx);
ParsedValue parse_struct(char *str, Json *arena, IntBuffer *buf, int buf_idx);
ParsedValue parse_json(char *str, Json *arena, IntBuffer *int_buf, int buf_idx, int arena_idx);

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char *skip_whitespace(char *json) {
    for (; is_whitespace(*json); json++)
        ;

    return json;
}

// ensures that a string is properly formatted, returning the end location of
// the string if it is correct and NULL if it is improperly formatted
//
// end location means the character after the string " terminator:
// "hello world",
// function will return where the comma is
char *validate_string(char *json) {
    bool backslashed = false;

    for (; *json != '\0'; json++) {
        char c = *json;

        if (c == '\\') {
            backslashed = true;
            continue;
        } else if (c == '"' && !backslashed) {
            return json + 1; // add 1 to go past the " character
        }

        backslashed = false;
    }

    return NULL;
}

// ensures that a number is properly formatted, returning the end location of
// the number and NULL if it is improperly formatted
//
// end location means the character after the last part of the number:
// 291028.298103,
// function will return where the comma is
char *validate_number(char *json) {
    bool finished_decimal = false;

    if (*json == '-') {
        json += 1;
    }
    // Numbers in json can _only_ match this regex [-]?[0-9]+\.[0-9]+ followed
    // by a comma or any whitespace

    for (; *json != '\0'; json++) {
        char c = *json;
        if (c == '.') {
            if (finished_decimal) {
                // A number like 1.0002.2 is invalid json!!
                return NULL;
            } else {
                finished_decimal = true;
                continue;
            }
        }
        if (is_whitespace(c) || c == ',') {
            return json;
        }
        if ('0' <= c && c <= '9') {
            continue;
        }
        if (c == '-') {
            return NULL;
        }

        // Some other character has been reached, number is over
        break;
    }
    // reached '\0', so this is a valid number
    return json;
}

char *validate_struct(char *json, IntBuffer *int_buf) {
    json = skip_whitespace(json);
    bool trailing_comma = false;

    int fields = 0;

    // Save the current index and add 1 so that we can insert our length
    // properly
    int buf_index = int_buf->length;
    buf_grow(int_buf, 1);
    int_buf->length += 1;

    while (*json != '}') {
        if (*json == '\0') {
            // if we make it to the end with no `}` then thats a json error
            return NULL;
        }

        // We need to make sure we have the field name string first
        if (*(json++) != '"') {
            return NULL;
        }
        json = validate_string(json);
        if (json == NULL) {
            return NULL;
        }
        json = skip_whitespace(json);

        // We need a : after the field: {"foo": true}
        if (*(json++) != ':') {
            return NULL;
        }
        json = skip_whitespace(json);

        // parse the next json item in the list
        json = validate_json(json, int_buf);
        if (json == NULL) {
            return NULL;
        }
        json = skip_whitespace(json);

        // ensure the json has a comma after the item. If it has no comma and we
        // already have a trailing comma, the json is invalid
        if (*json != ',') {
            if (trailing_comma) {
                return NULL;
            } else {
                trailing_comma = true;
            }
        } else {
            json++; // skip past the comma
        }
        json = skip_whitespace(json);

        fields++;
    }

    // fields + 1 to make room for null terminator at the end of the struct
    int_buf->data[buf_index] = fields + 1;

    return json + 1;
}

char *validate_list(char *json, IntBuffer *int_buf) {
    json = skip_whitespace(json);
    bool trailing_comma = false;

    int list_items = 0;

    // Save the current index and add 1 so that we can insert our length
    // properly
    int buf_index = int_buf->length;
    buf_grow(int_buf, 1);
    int_buf->length += 1;

    while (*json != ']') {
        if (*json == '\0') {
            // if we make it to the end with no `]` then thats a json error
            return NULL;
        }

        // parse the next json item in the list
        json = validate_json(json, int_buf);
        if (json == NULL) {
            return NULL;
        }
        json = skip_whitespace(json);

        // ensure the json has a comma after the item. If it has no comma and we
        // already have a trailing comma, the json is invalid
        if (*json != ',') {
            if (trailing_comma) {
                return NULL;
            } else {
                trailing_comma = true;
            }
        } else {
            json++; // skip past the comma
        }
        json = skip_whitespace(json);
        list_items++;
    }

    // list_items + 1 to make room for null terminator at the end of the list
    int_buf->data[buf_index] = list_items + 1;

    return json + 1;
}

char *validate_keyword(char *json) {
    json = skip_whitespace(json);
#define KEYWORD(v)                                                                                 \
    /* use sizeof(v) - 1 to remove the null terminator */                                          \
    if (strncmp(json, v, sizeof(v) - 1) == 0) {                                                    \
        return json + sizeof(v) - 1;                                                               \
    }
    KEYWORD("true");
    KEYWORD("false");
    KEYWORD("null");
#undef KEYWORD

    return NULL;
}

// Makes sure that the json string is properly formatted. Returns NULL is json
// is improperly formatted
//
// If the json is valid, the function will return a list of ints with a 0
// delimiter. This list will contain the number of elements in each level of the
// json.
//
//  { // 3 elements here
//      "foo": "bar",
//      "bar": { // 1 element here
//          "a": { // 3 elements here
//              "b": 2,
//              "c": 3,
//              "d": 4
//          }
//      },
//      "baz": [ // 5 elements here
//          [ // 0 elements here
//          ],
//          0,
//          1,
//          [ // 4 elements here
//              10,
//              9,
//              8,
//              7
//          ],
//          7,
//      ]
//  }
//
// In this example, the function will return [3, 1, 3, 5, 0, 4]. This is the
// order in which the numbers appear.
//
// in the case where the json is simply `"hello"` or `10`, the function will
// return []
char *validate_json(char *json, IntBuffer *int_buf) {
    json = skip_whitespace(json);

    switch (*json) {
    // json++ to skip past the [, {, or "
    case '[':
        return validate_list(json + 1, int_buf);
    case '{':
        return validate_struct(json + 1, int_buf);
    case '"':
        return validate_string(json + 1);
    }

    if (('0' <= *json && *json <= '9') || *json == '-' || *json == '.') {
        return validate_number(json);
    }
    if ('A' < *json && *json < 'z') {
        return validate_keyword(json);
    }

    return NULL;
}

Json *allocate_json(IntBuffer *buf) {
    // Initialize the amount of items we allocate to 1, all allocations will be
    // at LEAST 1 Json
    int len = 1;

    for (int i = 0; i < buf->length; i++) {
        len += buf->data[i];
    }

    return calloc(len, sizeof(Json));
}

ParsedValue parse_json(char *str, Json *arena, IntBuffer *buf, int buf_idx, int arena_idx) {
    str = skip_whitespace(str);
    struct ParsedValue;

    switch (*str) {
    // json++ to skip past the [, {, or "
    case '[':
        parse_list(str + 1, arena, buf, buf_idx);
    case '{':
        parse_struct(str + 1, arena, buf, buf_idx);
    case '"':
        parse_string(str + 1);
    }

    if (('0' <= *str && *str <= '9') || *str == '-' || *str == '.') {
        parse_number(str);
    }
    if ('A' < *str && *str < 'z') {
        parse_keyword(str);
    }

    return (ParsedValue) {0};
}

ParsedValue parse_string(char *str) {
    bool backslashed = false;

    char *start = str;

    int offset = 0;
    for (; *str != '\0'; str++) {
        char c = *str;

        if (c == '\\') {
            backslashed = true;
            continue;
        } else if (c == '"' && !backslashed) {
            return str + 1; // add 1 to go past the " character
        }

        backslashed = false;
    }

    return (ParsedValue) {0};
}

// Will return a pointer to the root element in a json string.
// If the json string is not properly formatted, the function will return NULL.
//
// The pointer returned can be freed by *just* calling free(ptr). This function
// only creates one allocation.
//
// The string used to parse the json *will* be mutated, do not expect it to be the
// same after calling this function.
Json *json_deserialize(char *str) {
    IntBuffer int_buf =
        (IntBuffer) {.data = malloc(INITIAL_CAPACITY), .capacity = INITIAL_CAPACITY, .length = 0};
    char *start = str;

    str = validate_json(str, &int_buf);
    if (str == NULL) {
        return NULL;
    }
    str = skip_whitespace(str);
    if (*str != '\0') {
        return NULL;
    }

    Json *arena = allocate_json(&int_buf);

    parse_json(start, arena, &int_buf, 0, 0);

    free(int_buf.data);

    return NULL;
}
