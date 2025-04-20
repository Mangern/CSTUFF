#include "fail.h"

#include "da.h"
#include "lex.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// fprintf(stream, "file:line:col: ")
void print_node_location(FILE* stream, node_t* node) {
    if (!node->leaf) {
        assert((da_size(node->children) > 0) && "non leafs should have children!");

        // First child should be earliest ? 
        print_node_location(stream, node->children[0]);
        return;
    }
    location_t loc = lexer_offset_location(node->pos.begin_offset);
    fprintf(stream, "%s:%d:%d: ", CURRENT_FILE_NAME, loc.line+1, loc.character+1);
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

void fail_token(token_t token) {
    location_t loc;
    loc = lexer_offset_location(token.begin_offset);
    fprintf(stderr, "%s:%d:%d: Unexpected token '%s'\n", CURRENT_FILE_NAME, loc.line+1, loc.character+1, TOKEN_TYPE_NAMES[token.type]);
    exit(1);
}

void fail_token_expected(token_t token, token_type_t expected) {
    location_t loc;
    loc = lexer_offset_location(token.begin_offset);
    fprintf(stderr, "%s:%d:%d: Expected '%s', got '%s'\n", CURRENT_FILE_NAME, loc.line+1, loc.character+1, TOKEN_TYPE_NAMES[expected], TOKEN_TYPE_NAMES[token.type]);
    exit(1);
}

