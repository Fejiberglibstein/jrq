#ifndef _EVAL_PRIVATE_H
#define _EVAL_PRIVATE_H

#include "src/eval.h"
#include "src/json.h"

#define EXPECT_TYPE(j, _type, free_list, ...)                                                      \
    ({                                                                                             \
        Json __j = j;                                                                              \
        if (__j.type != _type) {                                                                   \
            uint __list_size = sizeof(free_list) / sizeof(*free_list);                             \
            for (uint i = 0; i < __list_size; i++) {                                               \
                json_free((free_list)[i]);                                                         \
            }                                                                                      \
                                                                                                   \
            return json_invalid_msg(TYPE_ERROR(__VA_ARGS__));                                      \
        }                                                                                          \
        __j;                                                                                       \
    })

#define PROPOGATE_INVALID(j, free_list...)                                                         \
    ({                                                                                             \
        Json __j = j;                                                                              \
        if (__j.type == JSON_TYPE_INVALID) {                                                       \
            uint __list_size = sizeof(free_list) / sizeof(*free_list);                             \
            for (uint i = 0; i < __list_size; i++) {                                               \
                json_free((free_list)[i]);                                                         \
            }                                                                                      \
                                                                                                   \
            return __j;                                                                            \
        }                                                                                          \
        __j;                                                                                       \
    })

struct Variable {
    char *name;
    Json value;
};
typedef Vec(struct Variable) VariableStack;

typedef struct {
    /// The input json from invoking the program
    Json input;

    /// The variables that are currently in scope.
    ///
    /// This is represented as a stack so that we can allow for variable
    /// shadowing:
    ///
    ///  [{"foo": 4}].map(|v| v.foo.map(|v| v))
    ///
    /// will create a stack like [v: {"foo": 4}, v: 4].
    VariableStack vars;
} Eval;

Json eval_function_map(Eval *, ASTNode *);

#endif // _EVAL_PRIVATE_H
