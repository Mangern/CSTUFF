#include "da.h"
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
    "STRING_LITERAL",
    "PARENTHESIZED_EXPRESSION",
    "FUNCTION_CALL",
    "RETURN_STATEMENT"
};

node_t* node_create(node_type_t type) {
    node_t* node = malloc(sizeof(node_t));
    node->type = type;
    node->children = da_init(node_t*);
    node->symbol = NULL;
    return node;
}

static void print_tree_impl(node_t* node, int indent) {
    for (int i = 0; i < indent; ++i)
        printf(" ");
    printf("%s", NODE_TYPE_NAMES[node->type]);

    if (node->type == IDENTIFIER) {
        printf(" (%s)", node->data.identifier_str);
    } else if (node->type == OPERATOR) {
        printf(" (%s)", node->data.operator_str);
    } else if (node->type == INTEGER_LITERAL) {
        printf(" (%ld)", node->data.int_literal_value);
    } else if (node->type == STRING_LITERAL) {
        printf(" (%s)", node->data.string_literal_value);
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
