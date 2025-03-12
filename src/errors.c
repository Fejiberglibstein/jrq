#include "src/errors.h"
#include <stdarg.h>

JrqError jrq_error(Range r, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);

    return (JrqError) {.range = r, .err = msg};
}
