#include "da.h"
#include "lex.h"
#include "symbol.h"
#include "tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

node_t* root;

// This is the values from 
// https://en.cppreference.com/w/cpp/language/operator_precedence.html
const int OPERATOR_PRECEDENCE[] = {
    6,  // BINARY_ADD
    6,  // BINARY_SUB,
    5,  // BINARY_MUL,
    5,  // BINARY_DIV,
    5,  // BINARY_MOD,
    16, // BINARY_ASS_ADD,
    16, // BINARY_ASS_SUB,
    16, // BINARY_ASS_MUL,
    16, // BINARY_ASS_DIV,
    16, // BINARY_ASS_MOD,
    9,  // BINARY_GT, 
    9,  // BINARY_LT, 
    9,  // BINARY_GEQ,
    9,  // BINARY_LEQ,
    10,  // BINARY_EQ,
    10,  // BINARY_NEQ,
    1,  // BINARY_SCOPE_RES
    2,  // BINARY_DOT
    15, // BINARY_OR
    14, // BINARY_AND
    3,  // UNARY_SUB,
    3,  // UNARY_NEG
    3,  // UNARY_STAR,
    3,  // UNARY_DEREF TODO: don't know precedence..
};

char* NODE_TYPE_NAMES[] = {
    "LIST",
    "DECLARATION",
    "TYPE",
    "IDENTIFIER",
    "DECLARATION_LIST",
    "BLOCK",
    "OPERATOR",
    "INTEGER_LITERAL",
    "REAL_LITERAL",
    "STRING_LITERAL",
    "BOOL_LITERAL",
    "CHAR_LITERAL",
    "PARENTHESIZED_EXPRESSION",
    "FUNCTION_CALL",
    "RETURN_STATEMENT",
    "ASSIGNMENT_STATEMENT",
    "CAST_EXPRESSION",
    "IF_STATEMENT",
    "WHILE_STATEMENT",
    "ARRAY_INDEXING",
    "BREAK_STATEMENT",
    "CONTINUE_STATEMENT",
    "SCOPE_RESOLUTION",
    "DOT_ACCESS",
    "ALLOC_EXPRESSION"
};

char* OPERATOR_TYPE_NAMES[] = {
    "binary_add",
    "binary_sub",
    "binary_mul",
    "binary_div",
    "binary_mod",
    "binary_ass_add",
    "binary_ass_sub",
    "binary_ass_mul",
    "binary_ass_div",
    "binary_ass_mod",
    "binary_gt",
    "binary_lt",
    "binary_geq",
    "binary_leq",
    "binary_eq",
    "binary_neq",
    "binary_scope_res",
    "binary_dot",
    "binary_or",
    "binary_and",
    "unary_sub",
    "unary_neg",
    "unary_star",
    "unary_deref"
};

node_t* node_create(node_type_t type) {
    node_t* node = malloc(sizeof(node_t));
    node->type = type;
    node->children = NULL;
    node->symbol = NULL;
    node->type_info = NULL;
    node->pos = (token_t){0};
    node->leaf = false;
    return node;
}

node_t* node_create_leaf(node_type_t type, token_t token) {
    node_t* node = node_create(type);
    node->pos = token;
    node->leaf = true;
    return node;
}

node_t* node_deep_copy(node_t* node) {
    node_t* new_node = node_create(node->type);
    memcpy(new_node, node, sizeof(node_t));
    new_node->children = 0;

    for (size_t i = 0; i < da_size(node->children); ++i) {
        node_t* child_cpy = node_deep_copy(node->children[i]);
        da_append(new_node->children, child_cpy);
    }

    if (node->type == STRING_LITERAL && node->data.string_literal_value != NULL) {
        node->data.string_literal_value = strdup(node->data.string_literal_value);
    }

    if (node->type == IDENTIFIER && node->data.identifier_str != NULL) {
        node->data.identifier_str = strdup(node->data.identifier_str);
    }

    return new_node;
}

static void print_tree_impl(node_t* node, int indent) {
    for (int i = 0; i < indent; ++i)
        printf(" ");
    printf("%s", NODE_TYPE_NAMES[node->type]);

    if (node->type == IDENTIFIER) {
        printf(" (%s)", node->data.identifier_str);
    } else if (node->type == OPERATOR) {
        printf(" (%s)", OPERATOR_TYPE_NAMES[node->data.operator]);
    } else if (node->type == INTEGER_LITERAL) {
        printf(" (%ld)", node->data.int_literal_value);
    } else if (node->type == STRING_LITERAL) {
        if (global_string_list == NULL) {
            printf(" (%s)", node->data.string_literal_value);
        } else {
            printf(" (%s)", global_string_list[node->data.string_literal_idx]);
        }
    } else if (node->type == BOOL_LITERAL) {
        printf(" (%s)", node->data.bool_literal_value ? "true" : "false");
    }

    if (node->symbol != NULL) {
        printf(" -> [%s] %s", SYMBOL_TYPE_NAMES[node->symbol->type], node->symbol->name);
    }

    puts("");

    for (size_t i = 0; i < da_size(node->children); ++i) {
        print_tree_impl(node->children[i], indent + 2);
    }
}

void print_tree(node_t* node) {
    print_tree_impl(node, 0);
}

void node_find_range(node_t* node, range_t* range) {
    node_t *ptr_lft = node, *ptr_rgt = node;

    while (!ptr_lft->leaf) {
        ptr_lft = ptr_lft->children[0];
    }

    while (!ptr_rgt->leaf) {
        ptr_rgt = ptr_rgt->children[da_size(ptr_rgt->children) - 1];
    }

    range->start = lexer_offset_location(ptr_lft->pos.begin_offset);
    range->end   = lexer_offset_location(ptr_rgt->pos.end_offset - 1);

}

void node_add_child(node_t *parent, node_t *child) {
    da_append(parent->children, child);
    child->parent = parent;
}
