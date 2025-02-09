#ifndef _VECTOR_H
#define _VECTOR_H

#define Vec(t)                                                                                     \
    struct {                                                                                       \
        t *data;                                                                                   \
        size_t length;                                                                             \
        size_t capacity;                                                                           \
    }

#define vec_grow(vec, amt)                                                                         \
    if ((vec).capacity - (vec).length * sizeof(*(vec).data) < amt * sizeof(*(vec).data)) {         \
        do {                                                                                       \
            (vec).capacity = ((vec).capacity == 0) ? sizeof(*(vec).data) * 8 : (vec).capacity * 2; \
        } while ((vec).capacity - (vec).length * sizeof(*(vec).data) < amt * sizeof(*(vec).data)); \
        typeof((vec).data) tmp = (typeof((vec).data))realloc((void *)(vec).data, (vec).capacity);  \
        assert_ptr((void *)tmp);                                                                   \
        (vec).data = tmp;                                                                          \
    }

#define vec_append(vec, el...)                                                                     \
    do {                                                                                           \
        vec_grow((vec), 1);                                                                        \
        (vec).data[(vec).length++] = el;                                                           \
    } while (0)

#endif // _VECTOR_H
