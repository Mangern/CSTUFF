#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "da.h"

#include <stddef.h>

typedef struct regex_symbol_t regex_symbol_t;
typedef struct regex_t regex_t;

#define META_DIGIT_FLAG 0b1

enum REGEX_SYMBOL {
    REGOP,
    CLASS, // Several characters
    LPAREN,
    RPAREN
};

struct regex_symbol_t {
    enum REGEX_SYMBOL symbol_type;
    unsigned long value_0;
    unsigned long value_1;
    unsigned long value_2;
    unsigned long value_3;
};

struct regex_t {
    regex_symbol_t* symbols;
};

void regex_init(regex_t* regex);

void regex_preprocess(char* string, size_t length, regex_t* regex);

regex_symbol_t regex_symbol_at(regex_t* regex, size_t index);

// Add char to class
void class_set_char(regex_symbol_t* class_symbol, unsigned char c);

void class_set_range(regex_symbol_t* class_symbol, unsigned char lo, unsigned char hi);

// Check if char is in class
int class_has_char(regex_symbol_t* class_symbol, unsigned char c);

void regex_deinit(regex_t* regex);

#endif
