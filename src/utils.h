#ifndef _UTILS_H
#define _UTILS_H

#include <stdbool.h>

#define unreachable(str) assert(false && "unreachable: " str)

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
#endif // _UTILS_H
