#include "da.h"
#include "lex.h"
#include "symbol.h"
#include "tree.h"
#include <stdlib.h>
#include <stdio.h>

node_t* root;

char* NODE_TYPE_NAMES[] = {
    "LIST",
    "VARIABLE_DECLARATION",
    "TYPENAME",
    "IDENTIFIER",
    "FUNCTION_DECLARATION",
    "BLOCK",
    "OPERATOR",
    "INTEGER_LITERAL",
    "REAL_LITERAL",
    "STRING_LITERAL",
    "PARENTHESIZED_EXPRESSION",
    "FUNCTION_CALL",
    "RETURN_STATEMENT",
    "ASSIGNMENT_STATEMENT",
    "CAST_EXPRESSION",
    "IF_STATEMENT",
    "WHILE_STATEMENT",
};

char* OPERATOR_TYPE_NAMES[] = {
    "binary_add",
    "binary_sub",
    "binary_mul",
    "binary_div",
    "binary_gt",
    "binary_lt",
    "unary_sub",
    "unary_neg"
};

node_t* node_create(node_type_t type) {
    node_t* node = malloc(sizeof(node_t));
    node->type = type;
    node->children = da_init(node_t*);
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
        printf(" (%s)", global_string_list[node->data.string_literal_idx]);
    } else if (node->type == TYPENAME) {
        printf(" (%s)", node->data.typename_str);
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
