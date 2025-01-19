#include "./json.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int *data;
    int length;
    int capacity;
} IntBuffer;

inline void buf_grow(IntBuffer *str, int amt) {
    if (str->capacity - str->length < amt) {
        do {
            str->capacity *= 2;
        } while (str->capacity - str->length < amt);

        str->data = realloc(str->data, str->capacity);
    }
}

inline void buf_append(IntBuffer *str, int val) {
    buf_grow(str, 1);
    str->data[str->length++] = val;
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
//          0,
//          1,
//          [ // 4 elements here
//              10,
//              9,
//              8,
//              7
//          ],
//          7,
//          9
//      ]
//  }
//
//  In this example, the function will return [3, 1, 3, 5, 4]. This is the order
//  in which the numbers appear.
int *validate_json(char *json) {

    return NULL;
}

Json *json_deserialize(char *json) {

    validate_json(json);

    return NULL;
}
