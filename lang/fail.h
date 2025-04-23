#ifndef FAIL_H
#define FAIL_H

#include "langc.h"
#include "lex.h"

#include <stdio.h>

// fprintf(stream, "file:line:col: ")
void print_node_location(FILE* stream, node_t* node);

//  9 | void foo() {
//           ^~~
void print_visual_node_error(FILE* stream, node_t* node);

#define fail_node(node, ...) do {\
        print_node_location(stderr, node);\
        fprintf(stderr, __VA_ARGS__);\
        fprintf(stderr, "\n");\
        print_visual_node_error(stderr, node);\
        exit(EXIT_FAILURE);\
    } while (0);


char* location_str(location_t loc);

void fail_token(token_t);
void fail_token_expected(token_t, token_type_t);

#endif // FAIL_H
