#include "lex.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

void lexer_init(lexer_t* lexer, char* input, size_t length) {
    lexer->text = input;
    lexer->length = length;
    lexer->ptr = 0;
}

static bool iswhitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f';
}

Token lexer_advance(lexer_t* lexer) {
    // Skip whitespace
    while (lexer->ptr < lexer->length && iswhitespace(lexer->text[lexer->ptr]))++lexer->ptr;
    if (lexer->ptr == lexer->length) return EOF_T;

    Token ret;
    switch (lexer->text[lexer->ptr]) {
        case '+':
            ret = PLUS;
            break;
        case '-':
            ret = MINUS;
            break;
        case 'x':
            ret = X;
            break;
        case 'y':
            ret = Y;
            break;
        case '(':
            ret = LPAREN;
            break;
        case ')':
            ret = RPAREN;
            break;
        default:
            fprintf(stderr, "Unexpected character %c at position %zu\n", lexer->text[lexer->ptr], lexer->ptr);
            exit(1);
    }
    lexer->ptr++;
    return ret;
}

char* token_str(Token token) {
    switch (token) {
    case PLUS:
        return "+";
    case MINUS:
        return "-";
    case LPAREN:
        return "(";
    case RPAREN:
        return ")";
    case X:
        return "x";
    case Y:
        return "y";
    case EOF_T:
        return "$";
    case NUM_TOKENS:
        return "NUM_TOKENS";
    }
    fprintf(stderr, "Unexpected token %d in token_str\n", token);
    exit(1);
}
