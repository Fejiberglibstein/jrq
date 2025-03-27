#include "src/errors.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

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
    char *err_start = start;
    char *err_end = start;

    // First character of the line for the range start's line
    char *start_line = start;
    // First character of the line for the range end's line
    char *end_line = start;

    int lines = 1;

    // Last character of the line of the range's end line
    char *end;

    int highest_width = 0;
    int current_width = 0;
    for (end = start; *end != '\0'; end++) {
        if (lines > err.range.end.line) {
            break;
        }
        current_width += 1;

        // Increment start line until we get to the \n that matches the number
        // of lines of start.line
        if (lines < err.range.start.line) {
            start_line++;
        }

        // Same thing here
        if (lines < err.range.end.line) {
            end_line++;
        }

        if (*end == '\n') {
            if (current_width > highest_width) {
                highest_width = current_width;
            }
            current_width = 0;
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

    const int MARGIN = 5;

    return (struct error_msg_data) {
        .err_end = err_end,
        .err_start = err_start,
        .height = err.range.end.line - err.range.start.line + 1,
        .width = highest_width,

        .margin_start = MAX(start - MARGIN - 1, start_line),
        .margin_end = MIN(end_line + MARGIN - 1, end),

    };
}

char *jrq_error_format(JrqError err, char *text) {

    char *start_line = text;
    char *end_line = text;
    char *text_end = text;

    int new_lines = 1;
    // move start_line and end_line to the correct line.
    for (text_end = text; new_lines <= err.range.end.line && *text_end != '\0'; text_end++) {
        if (new_lines < err.range.start.line) {
            start_line++;
        }
        if (new_lines < err.range.end.line) {
            end_line++;
        }
        if (*text_end == '\n') {
            new_lines++;
        }
    }
    // Go past the new lines
    if (*start_line == '\n') {
        start_line++;
    }
    end_line++;
    text_end--;

    char *start = start_line + err.range.start.col;
    char *end = end_line + err.range.end.col;

    const int MARGIN = 5;

    char *start_margin = MAX(start - MARGIN - 1, start_line);
    char *end_margin = MIN(end + MARGIN - 1, text_end);

    size_t err_len = end - start;
    char *arrow_txt = malloc(err_len + 1);
    memset(arrow_txt, '~', err_len);
    arrow_txt[0] = '^';
    arrow_txt[err_len] = '\0';

    uint margin_len = end_margin - start_margin;

#define ERROR                                                                                      \
    "%.*s\n" /* printing the code that has the error */                                            \
    "%*s\n"  /* printing the arrows pointing to what caused the error */                           \
    "%s\n",  /* printing the error message */                                                      \
        margin_len, start_margin, (int)(start - start_margin + err_len - 1), arrow_txt, err.err

    int length = snprintf(NULL, 0, ERROR);
    char *v = malloc(length);
    snprintf(v, length, ERROR);
#undef ERROR

    return v;
}
