#include "../src/json.h"
#include <stdio.h>
#include <stdlib.h>

#define FLAGS JSON_NO_COMPACT | JSON_COLOR

void test_validate_lists() {
    Json string = (Json) {
        .type = JSONTYPE_STRING,
        .v.String = "000000000000000000000000000000000000000000000000000000000000000",
    };

    char *data = json_serialize(&string, FLAGS);
    printf("%s\n\n\n", data);
    free(data);

    // clang-format off
    Json list_data[] = {
        (Json) {
            .type = JSONTYPE_STRING,
            .v.String = "hello",
        },
        (Json) {
            .type = JSONTYPE_LIST,
            .v.List = (Json[]) {
                (Json) {
                    .type = JSONTYPE_STRING,
                    .v.String = "hello",
                },
                (Json) {
                    .type = JSONTYPE_STRING,
                    .v.String = "world",
                },
                (Json) {
                    .type = JSONTYPE_STRING,
                    .v.String = "",
                },
                0 // end of list
            },
        },
        (Json) {
            .type = JSONTYPE_INT,
            .v.Int = 4,
        },
        (Json) {
            .type = JSONTYPE_STRING,
            .v.String = "hello",
        },
        0 // end of list
    };
    // clang-format on

    Json list = (Json) {.type = JSONTYPE_LIST, .v.List = list_data};

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

void test_validate_structs() {
    Json *inner_list = (Json[]) {
        (Json) {.type = JSONTYPE_STRING, .v.String = "Heyyy"},
        (Json) {
            .type = JSONTYPE_FLOAT,
            .v.Double = 3.4F,
        },
        (Json) {
            .type = JSONTYPE_INT,
            .v.Int = 4,
        },
        0,
    };
    // clang-format off
    //
    Json list = (Json) {
        .type = JSONTYPE_STRUCT,
        .v.Struct = (Json[]) {
            (Json) {
                .field_name = "foo",
                .type = JSONTYPE_INT,
                .v.Int = 2
            },
            (Json) {
                .field_name = "bar",
                .type = JSONTYPE_LIST,
                .v.List = inner_list,
            },
            (Json) {
                .field_name = "bazz",
                .type = JSONTYPE_NULL,
            },
            (Json) {
                .field_name = "nested object",
                .type = JSONTYPE_STRUCT,
                .v.Struct = (Json []) {
                    (Json) {
                        .field_name = "double foo",
                        .type = JSONTYPE_BOOL,
                        .v.Bool = true,
                    },
                    (Json) {
                        .field_name = "blehah",
                        .type = JSONTYPE_LIST,
                        .v.List = inner_list,
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
    test_validate_lists();
    test_validate_structs();
}
