#ifndef TREE_H
#define TREE_H

#include "langc.h"
#include "lex.h"
#include "type.h"
#include "symbol.h"

typedef enum {
    LIST,
    DECLARATION, //  children: [identifier, type] | [identifier, type, block] | [identifier, type, expression]
    TYPE,             // children: [identifier] | [declaration_list, type]
    IDENTIFIER,
    DECLARATION_LIST, // children [declaration ...]
    BLOCK,                // children: [block | return_statement | function_call | variable_declaration]
    OPERATOR,             // children: [expression] | [expression, expression]
    INTEGER_LITERAL,      // leaf
    REAL_LITERAL,         // leaf
    STRING_LITERAL,
    BOOL_LITERAL,         // leaf
    PARENTHESIZED_EXPRESSION, // children: [expression | parenthesized_expression]
    FUNCTION_CALL,            // children: [identifier, list[expression]] | [scope_resolution, list[expression]]
    RETURN_STATEMENT,      // children: [expression]
    ASSIGNMENT_STATEMENT,  // children: [identifier, expression] | [array_indexing, expression]
    CAST_EXPRESSION,       // children: [typename, expression]
    IF_STATEMENT,           // children: [expression, block] | [expression, block, block]
    WHILE_STATEMENT,
    ARRAY_INDEXING,         // children: [identifier, list[expression]]
    BREAK_STATEMENT,
    SCOPE_RESOLUTION, // children: [scope resolution, identifier] | [identifier, identifier]
    DOT_ACCESS
} node_type_t;

typedef enum operator_t {
    BINARY_ADD = 0,
    BINARY_SUB,
    BINARY_MUL,
    BINARY_DIV,
    BINARY_MOD,
    BINARY_ASS_ADD,
    BINARY_ASS_SUB,
    BINARY_ASS_MUL,
    BINARY_ASS_DIV,
    BINARY_ASS_MOD,
    BINARY_GT, 
    BINARY_LT, 
    BINARY_GEQ,
    BINARY_LEQ,
    BINARY_EQ, 
    BINARY_NEQ,
    BINARY_SCOPE_RES,
    BINARY_DOT,
    UNARY_SUB, 
    UNARY_NEG, 
    UNARY_STAR,
    UNARY_DEREF // .*
} operator_t;

extern const int OPERATOR_PRECEDENCE[];
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
        size_t string_literal_idx;
        double real_literal_value;
        bool bool_literal_value;
        char* identifier_str;
        char* typename_str;
        operator_t operator;
        type_class_t type_class;
    } data;

    bool leaf;
    token_t pos;

    // Only in use if type == IDENTIFIER
    symbol_t* symbol;
};

// root: LIST node of either FUNCTION_DECLARATION or VARIABLE_DECLARATION
extern node_t* root;

node_t* node_create(node_type_t type);
node_t* node_create_leaf(node_type_t type, token_t token);

// Returns INCLUSIVE range (first and last character)
void node_find_range(node_t* node, range_t* range);

void print_tree(node_t* node);

#endif // TREE_H
