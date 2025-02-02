#include "./lexer.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct {
    Token *data;
    uint length;
    uint capacity;
} TokenBuf;

char *next_char(Lexer *l) {
    if (*l->str == '\n') {
        l->position.line += 1;
        l->position.col = 0;
    }
    l->position.col++;
    return ++l->str;
}

#define peek_char(l) (l)->str[1]
#define char(l) *(l)->str

static void skip_whitespace(Lexer *l) {
    while (is_whitespace(*l->str)) {
        next_char(l);
    }
}

static bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

static bool is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static LexResult parse_ident(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    while (is_alpha(peek_char(l)) || is_digit(peek_char(l))) {
        next_char(l);
    }

    Position end_position = l->position;

    uint size = (uint)(l->str - start) + 1;
    char *ident = calloc(1, size + 1);
    assert_ptr(ident);
    strncpy(ident, start, size);

    next_char(l);

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_IDENT, 
            .inner.ident = ident,
            .range = (Range) {
                .start = start_position,
                .end = end_position,
            }
        },
    };
}

static LexResult parse_string(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    bool backslashed = false;

    for (;;) {
        char c = *next_char(l);
        if (c == '"' && !backslashed) {
            break;
        }
        backslashed = false;
        if (c == '\\') {
            backslashed = true;
        }
        if (c == '\0') {
            return (LexResult) {.error_message = "Unterminated string"};
        }
    }

    Position end_position = l->position;

    uint size = (uint)(l->str - start) + 1;

    char *string = calloc(1, size + 1);
    assert_ptr(string);
    strncpy(string, start, size);

    // Skip past the "
    next_char(l);

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_STRING, 
            .inner.string = string,
            .range = (Range) {
                .start = start_position,
                .end = end_position,
            }
        },
    };
}

static LexResult parse_number(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;
    bool has_decimal = false;

    for (;;) {
        char c = peek_char(l);

        if (c == '.') {
            if (has_decimal) {
                return (LexResult) {.error_message = "Invalid suffix on decimal"};
            } else {
                has_decimal = true;
            }
        } else if (!is_digit(c)) {
            break;
        }

        next_char(l);
    }

    Position end_position = l->position;

    uint size = (uint)(l->str - start) + 1;
    char *number = calloc(1, size + 1);
    assert_ptr(number);
    strncpy(number, start, size);

    // Go to character after the number
    next_char(l);

    if (has_decimal) {
        double res = atof(number);
        free(number);
        return (LexResult) {
            .token = (Token) {
                .type = TOKEN_DOUBLE, 
                .inner.Double = res,
                .range = (Range) {
                    .start = start_position,
                    .end = end_position,
                }
            },
        };
    } else {
        int res = atoi(number);
        free(number);
        return (LexResult) {
            .token = (Token) {
                .type = TOKEN_INT, 
                .inner.Int = res,
                .range = (Range) {
                    .start = start_position,
                    .end = end_position,
                }
            },
        };
    }
}

static LexResult
parse_double_char(Lexer *l, TokenType single_type, char next, TokenType double_type) {
    Position start_position = l->position;
    char n = *next_char(l);

    if (n == next) {
        next_char(l);
        return (LexResult) {
            .token = (Token) {
                .type = double_type,
                .range = (Range) {
                    .start = start_position,
                    .end = l->position,
                },
            },
        };
    }

    return (LexResult) {
        .token = (Token) {
            .type = single_type,
            .range = (Range) {
                .start = start_position,
                .end = l->position,
            },
        },
    };
}

static LexResult parse_single_char(Lexer *l, TokenType type) {
    Position start_position = l->position;
    next_char(l);
    return (LexResult) {
        .token = (Token) {
            .type = type,
            .range = (Range) {
                .start = start_position,
                .end = l->position,
            },
        },
    };
}

LexResult lex_next_tok(Lexer *l) {
    skip_whitespace(l);
    switch (*l->str) {
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
        return parse_ident(l);
    case '"':
        return parse_string(l);
    case '0' ... '9':
        return parse_number(l);
        // clang-format off
    case '+': return parse_single_char(l, TOKEN_PLUS);
    case '-': return parse_single_char(l, TOKEN_MINUS);
    case '*': return parse_single_char(l, TOKEN_ASTERISK);
    case '/': return parse_single_char(l, TOKEN_SLASH);
    case '%': return parse_single_char(l, TOKEN_PERC);
    case ',': return parse_single_char(l, TOKEN_COMMA);
    case '.': return parse_single_char(l, TOKEN_DOT);
    case ';': return parse_single_char(l, TOKEN_SEMICOLON);
    case ':': return parse_single_char(l, TOKEN_COLON);
    case '{': return parse_single_char(l, TOKEN_LBRACE);
    case '}': return parse_single_char(l, TOKEN_RBRACE);
    case '(': return parse_single_char(l, TOKEN_LPAREN);
    case ')': return parse_single_char(l, TOKEN_RPAREN);
    case ']': return parse_single_char(l, TOKEN_RBRACKET);
    case '[': return parse_single_char(l, TOKEN_LBRACKET);

    case '=': return parse_double_char(l, TOKEN_NULL, '=', TOKEN_EQUAL); 
    case '!': return parse_double_char(l, TOKEN_BANG, '=', TOKEN_NOT_EQUAL);
    case '|': return parse_double_char(l, TOKEN_BAR, '|', TOKEN_OR);
    case '&': return parse_double_char(l, TOKEN_AMPERSAND, '&', TOKEN_AND);
    case '<': return parse_double_char(l, TOKEN_LANGLE, '=', TOKEN_EQUAL);
    case '>': return parse_double_char(l, TOKEN_RANGLE, '=', TOKEN_EQUAL);
        // clang-format on
    case '\0':
        return (LexResult) {.token.type = TOKEN_EOF};
    default:
        return (LexResult) {.error_message = "Illegal character"};
    }
}

Lexer lex_init(char *input) {
    return (Lexer) {
        .position = (Position) {
            .line = 1,
            .col = 1,
        },
        .str = input,
    };
}

static void tok_free(Token *tok) {
    switch (tok->type) {
    case TOKEN_IDENT:
        free(tok->inner.ident);
        break;
    case TOKEN_STRING:
        free(tok->inner.string);
        break;
    default:
        break;
    }
}
