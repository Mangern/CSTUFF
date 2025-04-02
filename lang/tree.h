#ifndef TREE_H
#define TREE_H

#include "langc.h"
#include "type.h"
#include "symbol.h"

typedef enum {
    LIST,
    VARIABLE_DECLARATION, // children: [typename, identifier] | [typename, identifier, expression]
    TYPENAME,             // leaf
    IDENTIFIER,
    FUNCTION_DECLARATION, // children: [identifier, list[variable_declaration], typename, block]
    BLOCK,                // children: [block | return_statement | function_call | variable_declaration]
    OPERATOR,             // children: [expression] | [expression, expression]
    INTEGER_LITERAL,      // leaf
    REAL_LITERAL,         // leaf
    STRING_LITERAL,
    PARENTHESIZED_EXPRESSION, // children: [expression | parenthesized_expression]
    FUNCTION_CALL,            // children: [identifier, list[expression]]
    RETURN_STATEMENT      // children: [expression]
} node_type_t;

enum operator_t {
    BINARY_ADD,
    BINARY_SUB,
    BINARY_MUL,
    BINARY_DIV,
    BINARY_GT,
    BINARY_LT,
    UNARY_SUB, // -
    UNARY_NEG, // !
};

extern char* NODE_TYPE_NAMES[];
extern char* OPERATOR_TYPE_NAMES[];

struct node_t {
    node_type_t type;

    node_t** children;

    type_info_t* type_info;

    // All pointers are owned.
    union {
        long int_literal_value;
        char* string_literal_value;
        double real_literal_value;
        char* identifier_str;
        char* typename_str;
        operator_t operator;
    } data;

    // Only in use if type == IDENTIFIER
    symbol_t* symbol;
};

// root: LIST node of either FUNCTION_DECLARATION or VARIABLE_DECLARATION
extern node_t* root;

node_t* node_create(node_type_t type);

void print_tree(node_t* node);

#endif // TREE_H
