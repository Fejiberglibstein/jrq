#include "src/errors.h"
#include "src/alloc.h"
#include "src/lexer.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

static const int MARGIN = 30;

JrqError jrq_error(Range r, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char *msg;
    vasprintf(&msg, fmt, args);
    va_end(args);

    return (JrqError) {.range = r, .err = msg};
}

struct error_msg_data {
    /// The start of the actual erroring code
    char *err_start;
    /// The end of the actual erroring code
    char *err_end;

    /// The start of the error, plus some margin at the beginning
    char *margin_start;
    /// The end of the error, plus some margin at the end
    char *margin_end;

    /// Total height (amount of lines) that the error makes up
    uint height;
    /// Total width (amount of characters) that the longest line of the error
    /// makes up
    uint width;
};

struct error_msg_data jrq_get_error_data(JrqError err, char *start) {
    // First character of the line for the range start's line
    char *start_line = start;
    // First character of the line for the range end's line
    char *end_line = start;

    int lines = 1;

    // Last character of the line of the range's end line
    char *end;

    int highest_width = 0;
    int current_width = 0;
    int width = 0;

    for (end = start; *end != '\0'; end++) {
        if (lines > err.range.end.line) {
            break;
        }

        // Increment start line until we get to the \n that matches the number
        // of lines of start.line
        if (lines < err.range.start.line) {
            start_line++;
        }

        width++;
        if (!(lines < err.range.start.line || width < err.range.start.col)
            && !(lines > err.range.end.line || width < err.range.end.col)) {
            current_width += 1;
            if (current_width >= highest_width) {
                highest_width = current_width;
            }
        }

        // Same thing here
        if (lines < err.range.end.line) {
            end_line++;
        }

        if (*end == '\n') {
            current_width = 0;
            width = 0;
            lines++;
        }
    }

    if (*start_line == '\n') {
        start_line++;
    }
    if (*end_line == '\n') {
        end_line++;
    }

    // Move back past the newline
    end--;

    char *err_end = end_line + err.range.end.col;
    // printf("\x1b[33m`%s`\x1b[0m\n", end_line);
    char *err_start = start_line + err.range.start.col;

    return (struct error_msg_data) {
        .err_end = err_end,
        .err_start = err_start,
        .height = err.range.end.line - err.range.start.line + 1,
        .width = highest_width - 1,

        .margin_start = MAX(err_start - MARGIN - 1, start_line),
        .margin_end = MIN(err_end + MARGIN - 1, end),

    };
}

char *jrq_error_format(JrqError err, char *text) {

    struct error_msg_data data = jrq_get_error_data(err, text);

    char *arrow_txt = jrq_malloc(data.width + 1);
    memset(arrow_txt, '~', data.width);

    arrow_txt[0] = '^';
    arrow_txt[data.width] = '\0';

    uint lmargin_len = data.err_start - data.margin_start - 1;
    uint rmargin_len = data.err_end - data.margin_end;
    uint err_len = data.err_end - data.err_start + 1;

    uint offset = (data.height > 1) ? 0 : data.err_start - data.margin_start;

#define ERROR                                                                                      \
    "%s\n"                            /* printing the error message */                             \
    "%.*s\x1b[4;31m%.*s\x1b[0m%.*s\n" /* printing the code that has the error */                   \
    "%*s\n",                          /* printing the arrows pointing to what caused the error */  \
        err.err,                      /* */                                                        \
        lmargin_len, data.margin_start, err_len, data.err_start - 1, rmargin_len,                  \
        data.err_end,                             /* */                                            \
        (int)(offset + data.width - 1), arrow_txt /* */

    int length = snprintf(NULL, 0, ERROR);
    char *v = malloc(length);
    snprintf(v, length, ERROR);
#undef ERROR

    free(err.err);
    free(arrow_txt);

    return v;
}
