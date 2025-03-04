#include "preprocess.h"
#include "da.h"

#include "nfa.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define FAIL(s) {fprintf(stderr, "%s\n", s); exit(-1);}

regex_symbol_t empty_class_symbol() {
    regex_symbol_t symbol = {
        .symbol_type = CLASS,
        .value_0 = 0,
        .value_1 = 0,
        .value_2 = 0,
        .value_3 = 0
    };
    return symbol;
}

void regex_init(regex_t* regex) {
    regex->symbols = malloc(sizeof(da_t));
    da_init(regex->symbols, sizeof(regex_symbol_t));
}

void regex_preprocess(char* string, size_t length, regex_t* regex) {
    da_t paren_stack;
    da_init(&paren_stack, sizeof(size_t));

    for (int i = 0; i < length;) {
        char c = string[i];

        switch (c) {
            case '\\': {
                if (i == length - 1) {
                    FAIL("Trailing escape character");
                }
                ++i;
                char next = string[i];
                if (next == 'd') {
                    regex_symbol_t symbol = empty_class_symbol();
                    class_set_range(&symbol, '0', '9');
                    da_push_back(regex->symbols, &symbol);
                } else if (next == 'w') {
                    regex_symbol_t symbol = empty_class_symbol();
                    class_set_range(&symbol, 'A', 'Z');
                    class_set_range(&symbol, 'a', 'z');
                    class_set_range(&symbol, '0', '9');
                    class_set_char(&symbol, '_');
                    da_push_back(regex->symbols, &symbol);
                } else {
                    // Escaped character: always character
                    regex_symbol_t symbol = empty_class_symbol();
                    class_set_char(&symbol, next);
                    da_push_back(regex->symbols, &symbol);
                }
                ++i;
            } break;
            case '?':
            case '*':
            case '+':
            case '|': {
                regex_symbol_t symbol = {
                    .symbol_type = REGOP,
                    .value_0 = string[i]
                };
                da_push_back(regex->symbols,&symbol);
                ++i;
            } break;
            case '.': {
                regex_symbol_t symbol = empty_class_symbol();
                class_set_range(&symbol, 0, 255);
                da_push_back(regex->symbols, &symbol);
                ++i;
            } break;
            case '(': {
                regex_symbol_t symbol = {
                    .symbol_type = LPAREN,
                    .value_0 = 0
                };
                da_push_back(&paren_stack, &(regex->symbols->size));
                da_push_back(regex->symbols, &symbol);
                ++i;
            } break;
            case ')': {
                if (paren_stack.size == 0) {
                    FAIL("Unmatched ')'");
                }
                size_t matching_index = *(size_t*)da_at(&paren_stack, paren_stack.size - 1);
                da_pop_back(&paren_stack);

                regex_symbol_t symbol = {
                    .symbol_type = RPAREN,
                    .value_0 = matching_index
                };

                regex_symbol_t* matching_lparen = da_at(regex->symbols, matching_index);
                matching_lparen->value_0 = regex->symbols->size;

                da_push_back(regex->symbols, &symbol);
                ++i;
            } break;
            case '[': {
                int end_index = i+1;
                while (end_index < length && string[end_index] != ']')++end_index;
                if (end_index == length) {
                    FAIL("Unmatched '['");
                }
                if (end_index == i + 1) {
                    FAIL("Empty class [] not allowed.");
                }
                ++i;
                regex_symbol_t symbol = empty_class_symbol();
                // TODO: handle ] in class
                while (i < end_index) {
                    // e.g. a-z
                    if (string[i+1] == '-' && i + 2 < end_index) {
                        class_set_range(&symbol, string[i], string[i+2]);
                        i += 3;
                    } else {
                        class_set_char(&symbol, string[i]);
                        ++i;
                    }
                }
                da_push_back(regex->symbols, &symbol);
                i = end_index + 1;
            } break;
            case ']': {
                FAIL("Unmatched ']'");
            } break;
            default: {
                regex_symbol_t symbol = empty_class_symbol();
                class_set_char(&symbol, string[i]);
                da_push_back(regex->symbols, &symbol);
                ++i;
            } break;
        }
    }

    if (paren_stack.size > 0) {
        FAIL("Unmatched '('");
    }

    da_deinit(&paren_stack);
}

regex_symbol_t regex_symbol_at(regex_t* regex, size_t index) {
    return *(regex_symbol_t*)da_at(regex->symbols, index);
}

void class_set_char(regex_symbol_t* class_symbol, unsigned char c) {
    assert(c != (unsigned char)NFA_TRANS_EPS);
    int index = c;
    int group = index / 64;
    assert(group < 4);
    int offset = index % 64;
    unsigned long *val;
    if (group == 0)val = &(class_symbol->value_0);
    if (group == 1)val = &(class_symbol->value_1);
    if (group == 2)val = &(class_symbol->value_2);
    if (group == 3)val = &(class_symbol->value_3);
    (*val) |= (1ULL<<offset);
}

void class_set_range(regex_symbol_t *class_symbol, unsigned char lo, unsigned char hi) {
    if (lo > hi) {
        fprintf(stderr, "Invalid range %c-%c\n", lo, hi);
        exit(-1);
    }
    for (int c = lo; c <= hi; ++c) {
        if ((unsigned char)c == (unsigned char)NFA_TRANS_EPS) continue;
        class_set_char(class_symbol, c);
    }
}

int class_has_char(regex_symbol_t* class_symbol, unsigned char c) {
    int index = c;
    int group = index / 64;
    assert(group < 4);
    int offset = index % 64;
    unsigned long val;
    if (group == 0)val = (class_symbol->value_0);
    if (group == 1)val = (class_symbol->value_1);
    if (group == 2)val = (class_symbol->value_2);
    if (group == 3)val = (class_symbol->value_3);
    return (val >> offset) & 1;
}

void regex_deinit(regex_t* regex) {
    da_deinit(regex->symbols);
    free(regex->symbols);
}
