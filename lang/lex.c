#include "lex.h"
#include "da.h"
#include "fail.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

char* TOKEN_TYPE_NAMES[] = {
    "typename",
    "identifier",
    ";",
    ":",
    ":=",
    "->",
    "{",
    "}",
    "(",
    ")",
    "[",
    "]",
    ",",
    "return",
    "integer literal",
    "real literal",
    "operator",
    "string literal",
    "char literal",
    "cast",
    "if",
    "else",
    "while",
    "true",
    "false",
    "break",
    "EOF"
};

int content_ptr;
int content_size;
char* content;
int current_line;
int* offset_line_number = 0; // for each position of the file, records which line it is
int* line_start = 0;         // for each line, records which offset it starts.

static void skip_whitespace();
static void skip_comments();
static bool isidentifierchar(char c);
static bool matches_prefix_word(char* pref);
static size_t matches_typename();
static size_t matches_identifier();
static size_t matches_integer();
static size_t matches_real();
static size_t matches_string();
static size_t matches_operator();
static void catchup_lines(size_t);

char* CURRENT_FILE_NAME;

void lexer_init(char* file_name, char* file_content) {
    CURRENT_FILE_NAME = file_name;

    content = file_content;
    content_size = da_size(content);

    da_append(line_start, 0);
}

token_t current_token = (token_t){.begin_offset = -1};

token_t lexer_peek() {
    if (current_token.begin_offset == content_ptr) return current_token;

    skip_whitespace();
    skip_comments();

    size_t match_len;
    current_token.begin_offset = content_ptr;
    if (content_ptr >= content_size) {
        current_token.type = LEX_END;
        current_token.end_offset = content_size;
    } else if (matches_prefix_word("return")) {
        current_token.type = LEX_RETURN;
        current_token.end_offset = content_ptr + 6;
    } else if (matches_prefix_word("cast")) {
        current_token.type = LEX_CAST;
        current_token.end_offset = content_ptr + 4;
    } else if (matches_prefix_word("if")) {
        current_token.type = LEX_IF;
        current_token.end_offset = content_ptr + 2;
    } else if (matches_prefix_word("else")) {
        current_token.type = LEX_ELSE;
        current_token.end_offset = content_ptr + 4;
    } else if (matches_prefix_word("while")) {
        current_token.type = LEX_WHILE;
        current_token.end_offset = content_ptr + 5;
    } else if (matches_prefix_word("true")) {
        current_token.type = LEX_TRUE;
        current_token.end_offset = content_ptr + 4;
    } else if (matches_prefix_word("false")) {
        current_token.type = LEX_FALSE;
        current_token.end_offset = content_ptr + 5;
    } else if (matches_prefix_word("break")) {
        current_token.type = LEX_BREAK;
        current_token.end_offset = content_ptr + 5;
    } else if ((match_len = matches_typename())) {
        current_token.type = LEX_TYPENAME;
        current_token.end_offset = content_ptr + match_len;
    } else if ((match_len = matches_identifier())) {
        current_token.type = LEX_IDENTIFIER;
        current_token.end_offset = content_ptr + match_len;
    } else if (content[content_ptr] == ';') {
        current_token.type = LEX_SEMICOLON;
        current_token.end_offset = content_ptr + 1;
    } else if (content_ptr + 1 < content_size && content[content_ptr] == ':' && content[content_ptr+1] == '=') {
        current_token.type = LEX_WALRUS;
        current_token.end_offset = content_ptr + 2;
    } else if (content_ptr + 1 < content_size && content[content_ptr] == '-' && content[content_ptr+1] == '>') {
        current_token.type = LEX_ARROW;
        current_token.end_offset = content_ptr + 2;
    } else if (content[content_ptr] == '(') {
        current_token.type = LEX_LPAREN;
        current_token.end_offset = content_ptr + 1;
    } else if (content[content_ptr] == ')') {
        current_token.type = LEX_RPAREN;
        current_token.end_offset = content_ptr + 1;
    } else if (content[content_ptr] == '{') {
        current_token.type = LEX_LBRACE;
        current_token.end_offset = content_ptr + 1;
    } else if (content[content_ptr] == '}') {
        current_token.type = LEX_RBRACE;
        current_token.end_offset = content_ptr + 1;
    } else if (content[content_ptr] == '[') {
        current_token.type = LEX_LBRACKET;
        current_token.end_offset = content_ptr + 1;
    } else if (content[content_ptr] == ']') {
        current_token.type = LEX_RBRACKET;
        current_token.end_offset = content_ptr + 1;
    } else if (content[content_ptr] == ',') {
        current_token.type = LEX_COMMA;
        current_token.end_offset = content_ptr + 1;
    } else if ((match_len = matches_integer())) {
        current_token.type = LEX_INTEGER;
        current_token.end_offset = content_ptr + match_len;
    } else if ((match_len = matches_real())) {
        current_token.type = LEX_REAL;
        current_token.end_offset = content_ptr + match_len;
    } else if ((match_len = matches_string())) {
        current_token.type = LEX_STRING;
        current_token.end_offset = content_ptr + match_len;
    } else if ((match_len = matches_operator())) {
        current_token.type = LEX_OPERATOR;
        current_token.end_offset = content_ptr + match_len;
    } else {
        // TODO: line number information
        fprintf(stderr, "%s: Unexpected character '%c'\n", location_str(lexer_offset_location(content_ptr)), content[content_ptr]);
        exit(1);
    }
    return current_token;
}

void lexer_advance() {
    lexer_peek(); // ensure current_token goes beyond
    content_ptr = current_token.end_offset;
}

char* lexer_substring(int begin_offset, int end_offset) {
    return strndup(&content[begin_offset], end_offset - begin_offset);
}

char* lexer_linedup(int line_num) {
    int line_start_offset = line_start[line_num];
    int line_end_offset = (line_num + 1 >= (int)da_size(line_start)) ? content_size : line_start[line_num + 1];
    return lexer_substring(line_start_offset, line_end_offset);
}

location_t lexer_offset_location(int offset) {
    catchup_lines(offset);

    location_t loc;
    loc.line = offset_line_number[offset];
    if (offset >= content_size) {
        loc.line = offset_line_number[da_size(offset_line_number) - 1];
    }
    loc.character = offset - line_start[loc.line];
    return loc;
}

// ===== Internal functions =====
static bool isidentifierchar(char c) {
    return (
        ('A' <= c && c <= 'Z')
     || ('a' <= c && c <= 'z')
     || ('0' <= c && c <= '9')
     || (c == '_')
    );
}

// Returns true if content[content_ptr:] matches pref and 
// content[len(pref)] is a word boundary
static bool matches_prefix_word(char* pref) {
    int len = strlen(pref);
    if (strncmp(&content[content_ptr], pref, len)) return false;
    return content_ptr+len >= content_size || !isidentifierchar(content[content_ptr+len]);
}

static size_t matches_typename() {
    if (matches_prefix_word("int")) return 3;
    if (matches_prefix_word("real")) return 4;
    if (matches_prefix_word("void")) return 4;
    if (matches_prefix_word("bool")) return 4;
    if (matches_prefix_word("char")) return 4;
    if (matches_prefix_word("string")) return 6;
    return 0;
}

static size_t matches_identifier() {
    if (!isalpha(content[content_ptr])) return 0;
    int ptr = content_ptr;
    while (ptr < content_size && isidentifierchar(content[ptr]))
        ++ptr;
    return ptr - content_ptr;
}

static size_t matches_integer() {
    if (!isdigit(content[content_ptr])) return 0;
    int ptr = content_ptr;
    while (ptr < content_size && isdigit(content[ptr]))
        ++ptr;
    // Real
    if (ptr < content_size && content[ptr] == '.')
        return 0;
    return ptr - content_ptr;
}

static size_t matches_real() {
    if (!isdigit(content[content_ptr])) return 0;
    int ptr = content_ptr;
    while (ptr < content_size && isdigit(content[ptr]))
        ++ptr;
    if (ptr < content_size && content[ptr] != '.')
        return 0;
    ++ptr;
    while (ptr < content_size && isdigit(content[ptr]))
        ++ptr;
    return ptr - content_ptr;
}

static size_t matches_string() {
    if (content[content_ptr] != '"') return 0;
    int ptr = content_ptr + 1;
    while (ptr < content_size && content[ptr] != '"') {
        if (content[ptr] == '\\')
            ptr += 2;
        else
            ptr += 1;
    }
    if (ptr >= content_size) {
        fprintf(stderr, "Unexpected EOF when parsing string literal\n");
        exit(EXIT_FAILURE);
    }
    ++ptr;
    return ptr - content_ptr;
}

/*
static size_t matches_char() {
    if (content[content_ptr] != '\'')return 0;
    int ptr = content_ptr + 1;
    while (ptr < content_size && content[ptr] != '\'') {
        if (content[ptr] == '\\')
            ptr += 2;
        else
            ptr += 1;
    }
    if (ptr >= content_size) {
        fprintf(stderr, "Unexpected EOF when parsing char literal\n");
        exit(EXIT_FAILURE);
    }
    ++ptr;
    return ptr - content_ptr;
}
*/

static size_t matches_operator() {
    // TODO: *=, **
    char c = content[content_ptr];
    switch (c) {
        // 1 char operators that also makes sense if followed by '='
        case '!':
        case '/':
        //case '~':
        //case '^':
            {
                if (content_ptr + 1 >= content_size) return 1;
                //if (content[content_ptr + 1] == '=') return 2;
                return 1;
            }
        // 1 char operators that also makes sense if followed by itself or '='
        case '+':
        case '-':
        case '*':
        case '<':
        case '>':
        case '%':
            {
                if (content_ptr + 1 >= content_size) return 1;
                //if (content[content_ptr+1] == c || content[content_ptr+1] == '=') return 2;
                return 1;
            }
        // double char operators
        case '=':
        case ':': // note: walrus is not an operator
            {
                if (content_ptr + 1 >= content_size) return 0;
                if (content[content_ptr + 1] == c) return 2;
                return 0;
            }
        default:
            return 0;
    }
}

static void catchup_lines(size_t goal_offset) {
    if (goal_offset > (size_t)content_size)goal_offset = (size_t)content_size;
    size_t current_ptr = da_size(offset_line_number);
    while (current_ptr <= goal_offset) {
        da_append(offset_line_number, current_line);
        ++current_ptr;
        if (current_ptr < (size_t)content_size && content[current_ptr] == '\n') {
            ++current_line;
            da_append(line_start, current_ptr+1);
        }
    }
}

static void skip_whitespace() {
    while (content_ptr < content_size && isspace(content[content_ptr])) {
        ++content_ptr;
    }
    catchup_lines(content_ptr);
}

static void skip_comments() {
    if (content[content_ptr] != '/') return;
    if (content_ptr + 1 < content_size && content[content_ptr+1] == '/') {
        // Single line comment
        while (content_ptr < content_size && content[content_ptr] != '\n')
            ++content_ptr;
    } else if (content_ptr + 1 < content_size && content[content_ptr + 1] == '*') {
        /*
         * Multi line comment
         */
        while (content_ptr + 1 < content_size) {
            if (content[content_ptr] == '*' && content[content_ptr+1] == '/') {
                content_ptr += 2;
                break;
            }
            ++content_ptr;
        }
    } else {
        return;
    }
    // Keep going until no more comment and no more whitespace
    skip_whitespace();
    skip_comments();
}
