#include "./lexer.h"
#include "src/alloc.h"
#include "utils.h"
#include <string.h>
#include <sys/types.h>

#define min(a, b) ((a) < (b)) ? (a) : (b)
#define max(a, b) ((a) > (b)) ? (a) : (b)

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

static LexResult parse_keyword(char *ident, Range range) {
#define KEYWORD(keyword, tok)                                                                      \
    if (strncmp(ident, keyword, sizeof(keyword) - 1) == 0)                                         \
        return (LexResult) {.token.type = (tok), .token.range = range};

    KEYWORD("true", TOKEN_TRUE);
    KEYWORD("false", TOKEN_FALSE);
    KEYWORD("null", TOKEN_NULL);
    return (LexResult) {.error_message = "not a keyword"};

#undef KEYWORD
}

static LexResult parse_ident(Lexer *l) {
    char *start = l->str;
    Position start_position = l->position;

    while (is_alpha(peek_char(l)) || is_digit(peek_char(l))) {
        next_char(l);
    }

    Position end_position = l->position;

    uint size = (uint)(l->str - start) + 1;
    char *ident = jrq_calloc(1, size + 1);
    strncpy(ident, start, size);

    next_char(l);

    Range range = (Range) {
        .start = start_position,
        .end = end_position,
    };

    LexResult keyword = parse_keyword(ident, range);
    if (keyword.error_message == NULL) {
        free(ident);
        return keyword;
    }

    return (LexResult) {
        .token = (Token) {
            .type = TOKEN_IDENT, 
            .inner.ident = ident,
            .range = range,
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

    uint size = (uint)(l->str - 1 - start);

    char *string = jrq_calloc(1, size + 1);
    strncpy(string, start + 1, size);

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
    char *number = jrq_calloc(1, size + 1);
    strncpy(number, start, size);

    // Go to character after the number
    next_char(l);

    if (has_decimal) {
        double res = atof(number);
        free(number);
        return (LexResult) {
            .token = (Token) {
                .type = TOKEN_NUMBER, 
                .inner.number = res,
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
                .type = TOKEN_NUMBER, 
                .inner.number = res,
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
        Position end_position = l->position;
        next_char(l);
        return (LexResult) {
            .token = (Token) {
                .type = double_type,
                .range = (Range) {
                    .start = start_position,
                    .end = end_position,
                },
            },
        };
    }

    return (LexResult) {
        .token = (Token) {
            .type = single_type,
            .range = (Range) {
                .start = start_position,
                .end = start_position,
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
                .end = start_position,
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

    case '=': return parse_double_char(l, TOKEN_INVALID, '=', TOKEN_EQUAL); 
    case '!': return parse_double_char(l, TOKEN_BANG, '=', TOKEN_NOT_EQUAL);
    case '|': return parse_double_char(l, TOKEN_BAR, '|', TOKEN_OR);
    case '&': return parse_double_char(l, TOKEN_AMPERSAND, '&', TOKEN_AND);
    case '<': return parse_double_char(l, TOKEN_LANGLE, '=', TOKEN_LT_EQUAL);
    case '>': return parse_double_char(l, TOKEN_RANGLE, '=', TOKEN_GT_EQUAL);
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

void parser_next(Parser *p) {
    if (p->error != NULL) {
        return;
    }
    LexResult t = lex_next_tok(p->l);
    if (t.error_message != NULL) {
        p->error = t.error_message;
        return;
    }

    if (p->should_free) {
        tok_free(&p->prev);
    }
    p->prev = p->curr;
    p->curr = t.token;
}

bool parser_matches(Parser *p, TokenType types[], int length) {
    if ((p)->error) {
        return false;
    }
    for (int i = 0; i < length; i++) {
        if (p->curr.type == types[i]) {
            parser_next(p);
            return true;
        }
    }
    return false;
}

void parser_expect(Parser *p, TokenType expected, char *err) {
    if (p->curr.type == expected) {
        parser_next(p);
        return;
    }
    p->error = err;
}

void tok_free(Token *tok) {
    switch (tok->type) {
    case TOKEN_IDENT:
        if (tok->inner.ident != NULL) {
            free(tok->inner.ident);
        }
        break;
    case TOKEN_STRING:
        if (tok->inner.string != NULL) {
            free(tok->inner.string);
        }
        break;
    default:
        break;
    }
}

Token_norange tok_norange(Token t) {
    return (Token_norange) {.inner = t.inner, .type = t.type};
}

inline Range range_combine(Range r1, Range r2) {
    return (Range) {
        .start = (Position) {
            .col = r1.start.col,
            .line = r1.start.line,
        },
        .end = (Position) {
            .col = r2.end.col,
            .line = r2.end.line,
        },
    };
}

void range_print(Range r) {
    printf("(%d:%d-%d:%d)\n", r.start.line, r.start.col, r.end.line, r.end.col);
}
