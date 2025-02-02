#ifndef _TEST_H
#define _TEST_H

#define jaq_assert(v, msg, args...)                                                                \
    if (!(v)) {                                                                                    \
        size_t buf_size = snprintf(NULL, 0, msg, args) + 1;                                        \
        char *res = malloc(buf_size);                                                              \
        snprintf(res, buf_size, msg, args);                                                        \
        return res;                                                                                \
    };


#endif // _TEST_H
