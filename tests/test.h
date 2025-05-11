#ifndef _TEST_H
#define _TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __jqr_assert(v, msg...)                                                                    \
    if (!(v)) {                                                                                    \
        size_t buf_size = snprintf(NULL, 0, msg) + 1;                                              \
        char *res = malloc(buf_size);                                                              \
        snprintf(res, buf_size, msg);                                                              \
        return res;                                                                                \
    }

#define __COMPARE_INT(exp, act, t) (exp t == act t)
#define __PRINT_INT(exp, act, t)                                                                   \
    (#t " not equal. Expected '%d' but got '%d'"), (int)exp t, (int)act t

#define __COMPARE_DOUBLE(exp, act, t) (fabs(exp t - act t) < 0.0001)
#define __PRINT_DOUBLE(exp, act, t) (#t " not equal. Expected '%f' but got '%f'"), exp t, act t

#define __COMPARE_STRING(exp, act, t) string_equal(exp t, act t)
#define __PRINT_STRING(exp, act, t)                                                                \
    (#t " not equal. Expected '%.*s' but got '%.*s'"), exp t.length, exp t.data, act t.length,     \
        act t.data

#define jqr_assert(type, exp, act, t)                                                              \
    __jqr_assert(__COMPARE_##type(exp, act, t), __PRINT_##type(exp, act, t))

#endif // _TEST_H
