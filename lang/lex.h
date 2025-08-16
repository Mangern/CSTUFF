#ifndef LEX_H
#define LEX_H

#include <stddef.h>
#include "langc.h"

typedef enum {
    LEX_IDENTIFIER,
    LEX_SEMICOLON,
    LEX_COLON,
    LEX_EQUAL,
    LEX_ARROW,
    LEX_LBRACE,
    LEX_RBRACE,
    LEX_LPAREN,
    LEX_RPAREN,
    LEX_LBRACKET,
    LEX_RBRACKET,
    LEX_COMMA,
    LEX_RETURN,
    LEX_INTEGER,
    LEX_REAL,
    LEX_OPERATOR,
    LEX_STRING,
    LEX_CHAR,
    LEX_CAST,
    LEX_IF,
    LEX_ELSE,
    LEX_WHILE,
    LEX_TRUE,
    LEX_FALSE,
    LEX_BREAK,
    LEX_CONTINUE,
    LEX_STRUCT,
    LEX_ALLOC,
    LEX_END
} token_type_t;

extern char* TOKEN_TYPE_NAMES[];
typedef struct {
    token_type_t type;
    int begin_offset;
    int end_offset;
} token_t;


extern char* CURRENT_FILE_NAME;

void lexer_init(char* file_name, char* file_content);

// Return a heap allocated substring of the file content.
char* lexer_substring(int begin_offset, int end_offset);

// 0-indexed line num plz
char* lexer_linedup(int line_num);

token_t lexer_peek();
void lexer_advance();

location_t lexer_offset_location(int offset);

#endif // LEX_H
