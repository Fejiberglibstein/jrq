#include "src/parser.h"
#include "src/lexer.h"
#include <string.h>

typedef struct {
    Token curr;
    Token prev;
    Lexer *l;
    char *error;
} Parser;

static void next(Parser *p) {
    LexResult t = lex_next_tok(p->l);
    if (t.error_message != NULL) {
        p->error = t.error_message;
        return;
    }

    p->prev = p->curr;
    p->curr = t.token;
}

static ASTNode expression(Parser *p) {
    return (ASTNode) {};
}

ParseResult ast_parse(Lexer *l) {
    Parser p = (Parser) {
        .l = l,
    };

    ASTNode node = expression(&p);
    if (p.error != NULL) {
        return (ParseResult) {.error_message = p.error};
    } else {
        return (ParseResult) {.node = node};
    }
}
