#include "fail.h"

#include "da.h"
#include "lex.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#define FAIL_BUF_SZ 1024
typedef struct diagnostic_t diagnostic_t;

FILE *out_stream;
enum FAIL_MODE fail_mode;

char fail_buf[FAIL_BUF_SZ];
jmp_buf FAIL_JMP_ENV;
diagnostic_t *diagnostics;

// fprintf(stream, "file:line:col: ")
void print_node_location(FILE* stream, node_t* node) {
    if (!node->leaf) {
        assert((da_size(node->children) > 0) && "non leafs should have children!");

        // First child should be earliest ? 
        print_node_location(stream, node->children[0]);
        return;
    }
    location_t loc = lexer_offset_location(node->pos.begin_offset);
    fprintf(stream, "%s: ", location_str(loc));
}

//  9 | void foo() {
//           ^~~
void print_visual_node_error(FILE* stream, node_t* node) {
    range_t node_range;
    node_find_range(node, &node_range);

    for (int line = node_range.start.line; line <= node_range.end.line; ++line) {
        char* line_str = lexer_linedup(line);
        fprintf(stream, "%5d | %s", line + 1, line_str);
        free(line_str);
    }

    if (node_range.start.line == node_range.end.line) {
        fprintf(stream, "      | ");
        for (int i = 0; i < node_range.start.character; ++i) {
            fprintf(stream, " ");
        }
        fprintf(stream, "^");
        for (int i = node_range.start.character + 1; i <= node_range.end.character; ++i) {
            fprintf(stream, "~");
        }
        fprintf(stream, "\n");
    }
}

char* location_str(location_t loc) {
    static char BUFFER[1024];
    snprintf(BUFFER, sizeof BUFFER, "%s:%d:%d", CURRENT_FILE_NAME, loc.line+1, loc.character + 1);

    return BUFFER;
}

void fail_token(token_t token) {
    if (fail_mode == FAIL_EXIT) {
        location_t loc;
        loc = lexer_offset_location(token.begin_offset);
        fprintf(stderr, "%s: Unexpected token '%s'\n", location_str(loc), TOKEN_TYPE_NAMES[token.type]);
        exit(1);
    } else {
        snprintf(fail_buf, FAIL_BUF_SZ, "Unexpected token '%s'", TOKEN_TYPE_NAMES[token.type]);
        range_t range;
        range.start = lexer_offset_location(token.begin_offset);
        range.end = lexer_offset_location(token.end_offset);
        diagnostic_t diag = {
            .message = strdup(fail_buf),
            .range = range
        };
        da_append(diagnostics, diag);
        longjmp(FAIL_JMP_ENV, 1);
    }
}

void fail_token_expected(token_t token, token_type_t expected) {
    if (fail_mode == FAIL_EXIT) {
        location_t loc;
        loc = lexer_offset_location(token.begin_offset);
        fprintf(stderr, "%s: Expected '%s', got '%s'\n",location_str(loc),  TOKEN_TYPE_NAMES[expected], TOKEN_TYPE_NAMES[token.type]);
        exit(1);
    } else {
        snprintf(fail_buf, FAIL_BUF_SZ, "Expected '%s', got '%s'", TOKEN_TYPE_NAMES[expected], TOKEN_TYPE_NAMES[token.type]);
        range_t range;
        range.start = lexer_offset_location(token.begin_offset);
        range.end = lexer_offset_location(token.end_offset);
        diagnostic_t diag = {
            .message = strdup(fail_buf),
            .range = range
        };
        da_append(diagnostics, diag);
        longjmp(FAIL_JMP_ENV, 1);
    }
}

void fail_node(node_t *node, const char *fmt, ...) {
    va_list args;
    if (fail_mode == FAIL_EXIT) {
        print_node_location(out_stream, node);
        va_start(args, fmt);
        vfprintf(out_stream, fmt, args);
        va_end(args);
        fprintf(out_stream, "\n");
        print_visual_node_error(out_stream, node);

        exit(EXIT_FAILURE);
    } else {
        va_start(args, fmt);
        vsnprintf(fail_buf, FAIL_BUF_SZ, fmt, args);
        va_end(args);

        range_t range;
        node_find_range(node, &range);
        // exclusive
        range.end.character++;
        diagnostic_t diag = {
            .message = strdup(fail_buf),
            .range = range
        };
        da_append(diagnostics, diag);
        longjmp(FAIL_JMP_ENV, 1);
    }
}

void fail_init_exit(FILE *stream) {
    fail_mode = FAIL_EXIT;
    out_stream = stream;
}

void fail_init_diagnostic() {
    fail_mode = FAIL_DIAGNOSTIC;
}
