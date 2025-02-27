#ifndef _EVAL_PRIVATE_H
#define _EVAL_PRIVATE_H

#include "src/eval.h"
#include "src/json.h"
#include "src/json_iter.h"
#include "src/parser.h"

#define _FREE_FREE_LIST(free_list)                                                                 \
    uint __list_size = sizeof(free_list) / sizeof(*free_list);                                     \
    for (uint i = 0; i < __list_size; i++) {                                                       \
        json_free((free_list)[i]);                                                                 \
    }

#define BUBBLE_ERROR(j, free_list)                                                                 \
    ({                                                                                             \
        EvalResult __r = j;                                                                        \
        if (__r.type == EVAL_ERR) {                                                                \
            _FREE_FREE_LIST(free_list)                                                             \
            return __r;                                                                            \
        }                                                                                          \
        __r;                                                                                       \
    })

#define EXPECT_TYPE(r, j, types, free_list, ...)                                                   \
    ({                                                                                             \
        Json __r = j;                                                                              \
        for (int __i = 0; __i < sizeof(types) / sizeof(*types); __i++) {                           \
            if (__r.type != types[__i]) {                                                          \
                _FREE_FREE_LIST(free_list);                                                        \
                return eval_res_error(r, TYPE_ERROR(__VA_ARGS__));                                 \
            }                                                                                      \
        }                                                                                          \
        __r;                                                                                       \
    })

#define EXPECT_JSON(range, j, free_list, ...)                                                      \
    /* Make sure that the json is not of type iterator */                                          \
    ({                                                                                             \
        EvalResult __r = BUBBLE_ERROR(j, free_list);                                               \
        if (__r.type == EVAL_ITER) {                                                               \
            _FREE_FREE_LIST(free_list);                                                            \
            return eval_res_error(range, TYPE_ERROR(__VA_ARGS__));                                 \
        }                                                                                          \
        __r.json;                                                                                  \
    })

#define EXPECT_ITER(range, j, free_list, ...)                                                      \
    /* Make sure that the json is not of type iterator */                                          \
    ({                                                                                             \
        EvalResult __r = BUBBLE_ERROR(j, free_list);                                               \
        if (__r.type == EVAL_JSON) {                                                               \
            _FREE_FREE_LIST(free_list);                                                            \
            return eval_res_error(range, TYPE_ERROR(__VA_ARGS__));                                 \
        }                                                                                          \
        __r.iter;                                                                                  \
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

EvalResult eval_node(Eval *e, ASTNode *node);
EvalResult eval_function_map(Eval *, ASTNode *);

EvalResult eval_res(Json);
EvalResult eval_res_error(Range r, char *format, ...);

Json vs_get_variable(VariableStack *vs, char *var_name, Range r);

#endif // _EVAL_PRIVATE_H
