#include "../src/json.h"
#include <stdio.h>
#include <stdlib.h>

#define FLAGS JSON_NO_COMPACT | JSON_COLOR

void test_validate_lists() {
    Json string = (Json) {
        .type = JSONTYPE_STRING,
        .v.string = "000000000000000000000000000000000000000000000000000000000000000",
    };

    char *data = json_serialize(&string, FLAGS);
    printf("%s\n\n\n", data);
    free(data);

    // clang-format off
    Json list_data[] = {
        (Json) {
            .type = JSONTYPE_STRING,
            .v.string = "hello",
        },
        (Json) {
            .type = JSONTYPE_LIST,
            .v.list = (Json[]) {
                (Json) {
                    .type = JSONTYPE_STRING,
                    .v.string = "hello",
                },
                (Json) {
                    .type = JSONTYPE_STRING,
                    .v.string = "world",
                },
                (Json) {
                    .type = JSONTYPE_STRING,
                    .v.string = "",
                },
                {0} // end of list
            },
        },
        (Json) {
            .type = JSONTYPE_NUMBER,
            .v.number = 4,
        },
        (Json) {
            .type = JSONTYPE_STRING,
            .v.string = "hello",
        },
        {0} // end of list
    };
    // clang-format on

    Json list = (Json) {.type = JSONTYPE_LIST, .v.list = list_data};

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
        (Json) {.type = JSONTYPE_STRING, .v.string = "Heyyy"},
        (Json) {
            .type = JSONTYPE_NUMBER,
            .v.number = 3.4F,
        },
        (Json) {
            .type = JSONTYPE_NUMBER,
            .v.number = 4,
        },
        {0},
    };
    Json list = (Json) {
        .type = JSONTYPE_OBJECT,
        .v.object = (Json[]) {
            (Json) {
                .field_name = "foo",
                .type = JSONTYPE_NUMBER,
                .v.number = 2
            },
            (Json) {
                .field_name = "bar",
                .type = JSONTYPE_LIST,
                .v.list = inner_list,
            },
            (Json) {
                .field_name = "bazz",
                .type = JSONTYPE_NULL,
            },
            (Json) {
                .field_name = "nested object",
                .type = JSONTYPE_OBJECT,
                .v.object = (Json []) {
                    (Json) {
                        .field_name = "double foo",
                        .type = JSONTYPE_BOOL,
                        .v.boolean = true,
                    },
                    (Json) {
                        .field_name = "blehah",
                        .type = JSONTYPE_LIST,
                        .v.list = inner_list,
                    },
                    {0},
                }
            },
            {0},
        },
    };

    char *data = json_serialize(&list, FLAGS);
    printf("%s\n\n", data);
    free(data);
}

int main() {
    test_validate_lists();
    test_validate_structs();
}
