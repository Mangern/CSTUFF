#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdbool.h>
#include <stddef.h>

#include "langc.h"

enum symbol_type_t {
    SYMBOL_GLOBAL_VAR,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER,
    SYMBOL_LOCAL_VAR
};

extern char* SYMBOL_TYPE_NAMES[];

struct symbol_t {
    char* name; // not owned
    symbol_type_t type;
    size_t sequence_number;
    node_t* node;
    bool is_builtin;

    // Mostly for debug/interpretation. Maybe for constant expression evaluation?
    union {
        long int_value;
        double real_value;
        char* string_value;
    } data;

    // Only if local, param or function
    // if function, points to its own
    symbol_table_t* function_symtable;
};

extern symbol_table_t* global_symbol_table;
extern char** global_string_list;

void create_symbol_tables();

#endif // SYMBOL_H
