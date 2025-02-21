#ifndef LEX_H
#define LEX_H

#include <stddef.h>

typedef enum {
    PLUS,
    MINUS,
    X,
    Y,
    STAR,
    LPAREN,
    RPAREN,
    EOF_T,
    NUM_TOKENS
} Token;

typedef struct lexer_t {
    char* text;
    size_t length;
    size_t ptr;
} lexer_t;

void lexer_init(lexer_t* lexer, char* input, size_t length);

Token lexer_advance(lexer_t* lexer);

char* token_str(Token token);

#endif // LEX_H
