#include "tree_transform.h"
#include "da.h"
#include "tree.h"
#include "type.h"

#include <assert.h>
#include <stdlib.h>

static void transform_pointer_indexing(node_t*);

// Hmm not sure how I want to do this
void tree_transform(node_t* node) {
    // Pre visit transforms

    for (size_t i = 0; i < da_size(node->children); ++i) {
        tree_transform(node->children[i]);
    }

    // Post visit transforms

    // Changes indexing a pointer to pointer arithmetic
    // which is actually disallowed by the type checking,
    // but we run after that muahahah.
    // Maybe I will shoot myself in the foot with this idk
    if ( node->type == ARRAY_INDEXING
      && node->children[0]->type_info->type_class == TC_POINTER) {
        transform_pointer_indexing(node);
    }
}

static void transform_pointer_indexing(node_t* node) {
    assert(da_size(node->children[1]->children) == 1); // from type check
    assert(node->parent != NULL);

    printf("Boutta transform\n");
    // change indexing [ identifier, list [ expression ]]
    // to     operator (deref) [ operator (add) [ identifier, operator (mul) [ expression, sizeof(ptr->inner) ] ] ]
    node_t* identifier_node = node->children[0];
    type_info_t* ptr_type = identifier_node->type_info;
    node_t* expression_node = node->children[1]->children[0];

    node_t* deref_node = node_create(OPERATOR);
    deref_node->data.operator = UNARY_DEREF;
    deref_node->parent = node->parent;

    node_t* add_node = node_create(OPERATOR);
    add_node->data.operator = BINARY_ADD;

    node_t* mul_node = node_create(OPERATOR);
    mul_node->data.operator = BINARY_MUL;

    node_t* literal_node = node_create(INTEGER_LITERAL);
    literal_node->data.int_literal_value = type_sizeof(ptr_type->info.info_pointer->inner);
    literal_node->type_info = expression_node->type_info;

    node_add_child(mul_node, expression_node);
    node_add_child(mul_node, literal_node);
    mul_node->type_info = expression_node->type_info;

    node_add_child(add_node, identifier_node);
    node_add_child(add_node, mul_node);
    add_node->type_info = mul_node->type_info;

    node_add_child(deref_node, add_node);
    deref_node->type_info = ptr_type->info.info_pointer->inner;

    long insert_idx = da_indexof(node->parent->children, &node);
    printf("Node: %p\n", node);
    printf("Parchild0: %p\n", node->parent->children[0]);
    assert(insert_idx >= 0);
    node->parent->children[insert_idx] = deref_node;

    // node should actually now be totally done :O
    // free(node); but fuck free'ing
}
