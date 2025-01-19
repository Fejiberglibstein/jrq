#include "./json.h"
#include <stdio.h>
#include <stdlib.h>

#define FLAGS  JSON_NO_COMPACT | JSON_COLOR

void test_lists() {
    Json string =
        (Json) {.type = TYPE_STRING,
                .string_type = "000000000000000000000000000000000000000000000000000000000000000"};

    char *data = json_serialize(&string, FLAGS);
    printf("%s\n\n\n", data);
    free(data);

    // clang-format off
    Json list_data[] = {
        (Json) {
            .type = TYPE_STRING,
            .string_type = "hello",
        },
        (Json) {
            .type = TYPE_LIST,
            .list_type = (Json[]) {
                (Json) {
                    .type = TYPE_STRING,
                    .string_type = "hello",
                },
                (Json) {
                    .type = TYPE_STRING,
                    .string_type = "world",
                },
                (Json) {
                    .type = TYPE_STRING,
                    .string_type = "",
                },
                0 // end of list
            },
        },
        (Json) {
            .type = TYPE_INT,
            .int_type = 4,
        },
        (Json) {
            .type = TYPE_STRING,
            .string_type = "hello",
        },
        0 // end of list
    };
    // clang-format on

    Json list = (Json) {.type = TYPE_LIST, .list_type = list_data};

    data = json_serialize(&list, FLAGS);
    printf("%s\n\n", data);
    free(data);

    list_data[1] = (Json) {0};
    data = json_serialize(&list, FLAGS);
    printf("%s\n\n", data);
    free(data);

    list_data[0] = (Json) {0};
    data = json_serialize(&list, FLAGS);
    printf("%s\n\n", data);
    free(data);
}

void test_structs() {
    Json *inner_list = (Json[]) {
        (Json) {.type = TYPE_STRING, .string_type = "Heyyy"},
        (Json) {
            .type = TYPE_FLOAT,
            .float_type = 3.4,
        },
        (Json) {
            .type = TYPE_INT,
            .int_type = 4,
        },
        0,
    };
    // clang-format off
    //
    Json list = (Json) {
        .type = TYPE_STRUCT,
        .struct_type = (Json[]) {
            (Json) {
                .field_name = "foo",
                .type = TYPE_INT,
                .int_type = 2
            },
            (Json) {
                .field_name = "bar",
                .type = TYPE_LIST,
                .list_type = inner_list,
            },
            (Json) {
                .field_name = "bazz",
                .type = TYPE_INT,
                .int_type = 2,
            },
            (Json) {
                .field_name = "nested object",
                .type = TYPE_STRUCT,
                .struct_type = (Json []) {
                    (Json) {
                        .field_name = "double foo",
                        .type = TYPE_INT,
                        .int_type = 10,
                    },
                    (Json) {
                        .field_name = "blehah",
                        .type = TYPE_LIST,
                        .list_type = inner_list,
                    },
                    0,
                }
            },
            0,
        }
    };
    // clang-format on

    char *data = json_serialize(&list, FLAGS);
    printf("%s\n\n", data);
    free(data);
}

int main() {
    /*test_lists();*/
    test_structs();
}
