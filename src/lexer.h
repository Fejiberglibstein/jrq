#ifndef _LEXER_H
#define _LEXER_H

#include <stdbool.h>
#include <sys/types.h>

#define RETURN_ERR(P, DEF)                                                                         \
    if (P->error != NULL) {                                                                        \
        return DEF;                                                                                \
    }

typedef enum uint8_t {
    TOKEN_INVALID, // Not a valid token

    TOKEN_EQUAL,     // ==
    TOKEN_NOT_EQUAL, // !=
    TOKEN_LT_EQUAL,  // <=
    TOKEN_GT_EQUAL,  // >=
    TOKEN_OR,        // ||
    TOKEN_AND,       // &&

    TOKEN_PLUS,      // +
    TOKEN_MINUS,     // -
    TOKEN_ASTERISK,  // *
    TOKEN_SLASH,     // /
    TOKEN_PERC,      // %
    TOKEN_BANG,      // !
    TOKEN_COMMA,     // ,
    TOKEN_DOT,       // .
    TOKEN_SEMICOLON, // ;
    TOKEN_BAR,       // |
    TOKEN_AMPERSAND, // &
    TOKEN_COLON,     // :

    TOKEN_LBRACE,   // {
    TOKEN_RBRACE,   // }
    TOKEN_LPAREN,   // (
    TOKEN_RPAREN,   // )
    TOKEN_LANGLE,   // <
    TOKEN_RANGLE,   // >
    TOKEN_LBRACKET, // [
    TOKEN_RBRACKET, // ]

    // All of these have a `.inner` in the token
    TOKEN_IDENT,
    TOKEN_STRING,
    TOKEN_NUMBER,

    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    TOKEN_EOF,

} TokenType;

union tok_inner {
    char *ident;
    char *string;
    double number;
};

typedef struct {
    uint line;
    uint col;
} Position;

typedef struct {
    Position start;
    Position end;
} Range;

typedef struct {
    union tok_inner inner;
    TokenType type;
} Token_norange;

typedef struct {
    union tok_inner inner;
    TokenType type;
    Range range;
} Token;

typedef struct {
    char *str;
    Position position;
} Lexer;

typedef struct {
    Token token;
    char *error_message;
} LexResult;

typedef struct {
    Token curr;
    Token prev;
    char *error;
    Lexer *l;
    bool should_free;
} Parser;

void parser_next(Parser *p);
bool parser_matches(Parser *p, TokenType types[], int length);
void parser_expect(Parser *p, TokenType expected, char *err);

Lexer lex_init(char *);
LexResult lex_next_tok(Lexer *);

void tok_free(Token *tok);
Token_norange tok_norange(Token t);

Range range_combine(Range, Range);
void range_print(Range);

#endif // _LEXER_H
