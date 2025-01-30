#include "./lexer.h"
#include "utils.h"

typedef struct {
    char *str;
    Position position;
} Lexer;

typedef struct {

} TokenBuf;

void skip_whitespace(Lexer *l) {
    while (is_whitespace(*l->str)) {
        if (*l->str == '\n') {
            l->position.line += 1;
            l->position.col = 0;
        }
        l->str++;
        l->position.col += 1;
    }
}

Token *lex(char *input) {
    Lexer l = (Lexer) {
        .position = (Position) {
            .line = 1,
            .col = 1,
        },
        .str = input,
    };
}
