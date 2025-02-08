#include "./json.h"
#include "./utils.h"
#include "vector.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#define INITIAL_CAPACITY 4

enum keyword {
    KEYWORD_TRUE,
    KEYWORD_FALSE,
    KEYWORD_NULL,
};

typedef Vec(int) Vec_int;

typedef struct {
    Vec_int buf;
    // The size of all strings in the json
    int str_len;
    // Pointer to the region of memory we allocate to hold the strings
    char *str_ptr;
    // Pointer to the region of memory we allocate to hold all the json
    Json *arena;
} JsonData;

typedef struct {
    char *end;
    Json json;
    // Optional error message when validating the json
    //
    // Null is there is no error
    char *error_message;
} ParsedValue;

typedef struct {
    char *res;
    char *err;
} ValidateResult;

ValidateResult validate_json(char *json, JsonData *data);

static ParsedValue parse_string(char *str, JsonData *data);
static ParsedValue parse_number(char *str, JsonData *_);
static ParsedValue parse_keyword(char *str, JsonData *_);
static ParsedValue parse_list(char *str, JsonData *data, int buf_idx);
static ParsedValue parse_struct(char *str, JsonData *data, int buf_idx);
static ParsedValue parse_json(char *, JsonData *, int, int, char *);

static char *skip_whitespace(char *json) {
    while (is_whitespace(*json)) {
        json++;
    }

    return json;
}

// ensures that a string is properly formatted, returning the end location of
// the string if it is correct and NULL if it is improperly formatted
//
// end location means the character after the string " terminator:
// "hello world",
// function will return where the comma is
ParsedValue parse_string(char *str, JsonData *data) {
    bool backslashed = false;
    char *new_str = NULL;
    char *start = str;

    for (int str_len = 0; *str != '\0'; str++, str_len++) {
        char c = *str;

        if (c == '\\') {
            backslashed = true;
            continue;
        } else if (c == '"' && !backslashed) {
            if (data->str_ptr == NULL) {
                data->str_len += str_len + 1;
            } else {
                new_str = data->str_ptr;
                data->str_ptr += str_len + 1;
                memcpy(new_str, start, str_len);
            }
            return (ParsedValue) {
                .end = str + 1, // add 1 to go past the " character
                .json = (Json) {
                    .type = JSONTYPE_STRING,
                    .inner.string = new_str,
                },
            };
        }

        backslashed = false;
    }
    // String not terminated, error
    return (ParsedValue) {.error_message = "Unterminated string"};
}

// ensures that a number is properly formatted, returning the end location of
// the number and NULL if it is improperly formatted
//
// end location means the character after the last part of the number:
// 291028.298103,
// function will return where the comma is
ParsedValue parse_number(char *str, JsonData *_) {
    bool finished_decimal = false;

    char *start = str;
    if (*str == '-') {
        str += 1;
    }
    // Numbers in json can _only_ match this regex [-]?[0-9]+\.[0-9]+ followed
    // by a comma or any whitespace

    for (; *str != '\0'; str++) {
        char c = *str;
        if (c == '.') {
            if (finished_decimal) {
                // A number like 1.0002.2 is invalid json!!
                return (ParsedValue) {.error_message = "Invalid number"};
            } else {
                finished_decimal = true;
                continue;
            }
        }
        if ('0' <= c && c <= '9') {
            continue;
        }
        if (c == '-') {
            return (ParsedValue) {.error_message = "Invalid number"};
        }

        // Some other character has been reached, number is over
        break;
    }

    int num_length = (int)(str - start);
    char buf[num_length + 1];
    memcpy(buf, start, num_length);
    buf[num_length] = '\0';

    return (ParsedValue) {
            .end = str,
            .json = (Json) {
                .type = JSONTYPE_NUMBER,
                .inner.number = atof(buf),
            },
        };
}

ParsedValue parse_keyword(char *json, JsonData *_) {
    json = skip_whitespace(json);
#define KEYWORD(v, l...)                                                                           \
    /* use sizeof(v) - 1 to remove the null terminator */                                          \
    if (strncmp(json, v, sizeof(v) - 1) == 0) {                                                    \
        return (ParsedValue) {.end = json + sizeof(v) - 1, .json = {l}};                           \
    }
    KEYWORD("true", .type = JSONTYPE_BOOL, .inner.boolean = true);
    KEYWORD("false", .type = JSONTYPE_BOOL, .inner.boolean = false);
    KEYWORD("null", .type = JSONTYPE_NULL);
#undef KEYWORD

    return (ParsedValue) {
        .end = "Invalid keyword",
    };
}

ParsedValue parse_list(char *str, JsonData *data, int buf_idx) {
    str = skip_whitespace(str);

    int list_items = 0;
    // start it at 1, we need to reserve the 0th index for the start of the json
    int base_arena_idx = 1;
    for (int i = 0; i < buf_idx; i++) {
        base_arena_idx += data->buf.data[i];
    }

    int current_length = (int)data->buf.length;
    data->buf.length += 1;

    while (*str != ']') {
        // The json is already validated, so we don't need to check if things
        // are validated like we did in the validator function

        // parse the next json item in the list
        ParsedValue ret = parse_json(
            str,
            data,
            buf_idx + (int)data->buf.length - current_length,
            base_arena_idx + list_items,
            NULL
        );
        str = ret.end;

        // Skip past the comma if there is one
        str = skip_whitespace(str);
        if (*str == ',') {
            str++;
        }
        str = skip_whitespace(str);

        list_items++;
    }

    data->arena[base_arena_idx + list_items] = (Json) {.type = JSONTYPE_END_LIST};

    return (ParsedValue) {
        .end = str + 1,
        .json = (Json) {
            .inner.list = data->arena + base_arena_idx,
            .type = JSONTYPE_LIST,
        },
    };
}

ParsedValue parse_struct(char *str, JsonData *data, int buf_idx) {
    str = skip_whitespace(str);

    int fields = 0;
    // start it at 1, we need to reserve the 0th index for the start of the json
    int base_arena_idx = 1;
    for (int i = 0; i < buf_idx; i++) {
        base_arena_idx += data->buf.data[i];
    }

    int current_length = data->buf.length;
    data->buf.length += 1;

    while (*str != '}') {
        // The json is already validated, so we don't need to check if things
        // are validated like we did in the validator function

        // skip past the " character
        str++;

        ParsedValue ret = parse_string(str, data);
        str = ret.end;
        char *field_name = ret.json.inner.string;

        // Skip past the colon after the field name: {"foo"   :   3}
        str = skip_whitespace(str);
        str++;
        str = skip_whitespace(str);

        // parse the next json item in the list
        ret = parse_json(
            str,
            data,
            buf_idx + data->buf.length - current_length,
            base_arena_idx + fields,
            field_name
        );
        str = ret.end;

        // Skip past the comma if there is one
        str = skip_whitespace(str);
        if (*str == ',') {
            str++;
        }
        str = skip_whitespace(str);

        fields++;
    }

    data->arena[base_arena_idx + fields] = (Json) {.type = JSONTYPE_END_LIST};

    return (ParsedValue) {
        .end = str + 1,
        .json = (Json) {
            .inner.object = data->arena + base_arena_idx,
            .type = JSONTYPE_OBJECT,
        },
    };
}

ValidateResult validate_struct(char *json, JsonData *data) {
    json = skip_whitespace(json);
    bool trailing_comma = false;

    int fields = 0;

    // Save the current index and add 1 so that we can insert our length
    // properly

    uint buf_index = data->buf.length;
    vec_grow(data->buf, 1);
    data->buf.length += 1;

    while (*json != '}') {
        if (*json == '\0') {
            // if we make it to the end with no `}` then thats a json error
            return (ValidateResult) {.err = "No closing } in json object"};
        }

        // We need to make sure we have the field name string first
        if (*(json++) != '"') {
            return (ValidateResult) {.err = "Invalid json object key"};
        }
        json = parse_string(json, data).end;
        if (json == NULL) {
            return (ValidateResult) {.err = "Invalid json object key"};
        }
        json = skip_whitespace(json);

        // We need a : after the field: {"foo": true}
        if (*(json++) != ':') {
            return (ValidateResult) {.err = "Missing ':' after key in json object"};
        }
        json = skip_whitespace(json);

        // parse the next json item in the list
        ValidateResult res = validate_json(json, data);
        if (res.err != NULL) {
            return res;
        }
        json = res.res;
        json = skip_whitespace(json);

        // ensure the json has a comma after the item. If it has no comma and we
        // already have a trailing comma, the json is invalid
        if (*json != ',') {
            if (trailing_comma) {
                return (ValidateResult) {.err = "Missing ',' between values"};
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
    data->buf.data[buf_index] = fields + 1;

    return (ValidateResult) {.res = json + 1};
}

ValidateResult validate_list(char *json, JsonData *data) {
    json = skip_whitespace(json);
    bool trailing_comma = false;

    int list_items = 0;

    // Save the current index and add 1 so that we can insert our length
    // properly
    int buf_index = (int)data->buf.length;
    vec_grow(data->buf, 1);
    data->buf.length += 1;

    while (*json != ']') {
        if (*json == '\0') {
            // if we make it to the end with no `]` then thats a json error
            return (ValidateResult) {.err = "No closing ] in json list"};
        }

        // parse the next json item in the list
        ValidateResult res = validate_json(json, data);
        if (res.err != NULL) {
            return res;
        }
        json = res.res;
        json = skip_whitespace(json);

        // ensure the json has a comma after the item. If it has no comma and we
        // already have a trailing comma, the json is invalid
        if (*json != ',') {
            if (trailing_comma) {
                return (ValidateResult) {.err = "Missing ',' between values"};
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
    data->buf.data[buf_index] = list_items + 1;

    return (ValidateResult) {.res = json + 1};
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
// order in which the numbers appear. However, all the numbers returned will
// have 1 added to them, so the function will actually return [4, 2, 4, 6, 1, 5]
//
// in the case where the json is simply `"hello"` or `10`, the function will
// return []
ValidateResult validate_json(char *json, JsonData *data) {
    json = skip_whitespace(json);

    ParsedValue res;

    switch (*json) {
    // json++ to skip past the [, {, or "
    case '[':
        return validate_list(json + 1, data);
    case '{':
        return validate_struct(json + 1, data);
    case '"':
        res = parse_string(json + 1, data);
        return (ValidateResult) {.res = res.end, .err = res.error_message};
    }

    if (('0' <= *json && *json <= '9') || *json == '-' || *json == '.') {
        res = parse_number(json, data);
        return (ValidateResult) {.res = res.end, .err = res.error_message};
    }
    if ('A' < *json && *json < 'z') {
        res = parse_keyword(json, data);
        return (ValidateResult) {.res = res.end, .err = res.error_message};
    }

    return (ValidateResult) {.err = "This was NOT json"};
}

void allocate_json(JsonData *data) {
    // Initialize the amount of items we allocate to 1, all allocations will be
    // at LEAST 1 Json
    int len = 1;

    for (int i = 0; i < data->buf.length; i++) {
        len += data->buf.data[i];
    }

    void *mem = calloc(len * sizeof(Json) + data->str_len, 1);
    assert_ptr(mem);
    data->arena = mem;
    // Skip past the json allocation part and put it in the string allocation
    // part
    data->str_ptr = mem + len * sizeof(Json);
}

ParsedValue parse_json(char *str, JsonData *data, int buf_idx, int arena_idx, char *field_name) {
    str = skip_whitespace(str);
    ParsedValue ret;

    switch (*str) {
    // json++ to skip past the [, {, or "
    case '[':
        ret = parse_list(str + 1, data, buf_idx);
        goto stop;
    case '{':
        ret = parse_struct(str + 1, data, buf_idx);
        goto stop;
    case '"':
        ret = parse_string(str + 1, data);
        goto stop;
    }

    if (('0' <= *str && *str <= '9') || *str == '-' || *str == '.') {
        ret = parse_number(str, data);
        goto stop;
    }
    if ('A' < *str && *str < 'z') {
        ret = parse_keyword(str, data);
        goto stop;
    }

stop:
    if (field_name != NULL) {
        ret.json.field_name = field_name;
    }

    data->arena[arena_idx] = ret.json;
    return ret;
}

// Will return a pointer to the root element in a json string.
// If the json string is not properly formatted, the function will return NULL.
//
// The pointer returned can be freed by *just* calling free(ptr). This function
// only creates one allocation.
Json *json_deserialize(char *str) {
    JsonData data = {
        .buf = (Vec_int) {
            .data = malloc(INITIAL_CAPACITY),
            .capacity = INITIAL_CAPACITY,
            .length = 0,
        },
        .str_len = 0,
        .arena = NULL,
        .str_ptr = NULL,
    };
    char *start = str;

    ValidateResult res = validate_json(str, &data);
    str = res.res;
    if (res.err != NULL) {
        fprintf(stderr, "jaq: Parse error: %s\n", res.err);
        free(data.buf.data);
        return NULL;
    }
    str = skip_whitespace(str);
    if (*str != '\0') {
        free(data.buf.data);
        return NULL;
    }

    allocate_json(&data);

    // Reset the length so that while parsing we can use it to store where to
    // place the next complex type (struct/lists)
    data.buf.length = 0;
    parse_json(start, &data, 0, 0, NULL);

    free(data.buf.data);
    return data.arena;
}
