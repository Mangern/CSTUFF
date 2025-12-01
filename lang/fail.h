#ifndef FAIL_H
#define FAIL_H

#include "langc.h"
#include "lex.h"

#include <stdio.h>
#include <setjmp.h>

enum FAIL_MODE {
    FAIL_EXIT,
    FAIL_DIAGNOSTIC
};

struct diagnostic_t {
    range_t range;
    char *message;
};

extern jmp_buf FAIL_JMP_ENV;
extern struct diagnostic_t *diagnostics;
extern enum FAIL_MODE fail_mode;

// fprintf(stream, "file:line:col: ")
void print_node_location(FILE* stream, node_t* node);

//  9 | void foo() {
//           ^~~
void print_visual_node_error(FILE* stream, node_t* node);

void fail_node(node_t *node, const char* fmt, ...);

char* location_str(location_t loc);

void fail_init_exit(FILE *stream);
void fail_init_diagnostic();

void fail_token(token_t);
void fail_token_expected(token_t, token_type_t);

void fail_character(location_t loc, char);

#endif // FAIL_H
