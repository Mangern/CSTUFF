#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "langc.h"
#include "symbol.h"
#include "symbol_table.h"
#include "tree.h"
#include "da.h"
#include "fail.h"


static void insert_builtin_functions();
static void create_function_tables(node_t*);
static void create_insert_variable_declaration(symbol_table_t*, symbol_type_t, node_t*);
static void bind_references(symbol_table_t*, node_t*);

char* SYMBOL_TYPE_NAMES[] = {
    "GLOBAL_VAR",
    "FUNCTION",
    "PARAMETER",
    "LOCAL_VAR"
};

symbol_table_t* global_symbol_table;

void create_symbol_tables() {

    global_symbol_table = symbol_table_init();

    insert_builtin_functions();

    for (size_t i = 0; i < da_size(root->children); ++i) {
        node_t* node = root->children[i];
        if (node->type == FUNCTION_DECLARATION) {
            create_function_tables(node);
        } else if (node->type == VARIABLE_DECLARATION) {
            create_insert_variable_declaration(global_symbol_table, SYMBOL_GLOBAL_VAR, node);
        } else {
            assert(false && "Unexpected node type in global statement list");
        }
    }
}

static void insert_builtin_functions() {
    {
        symbol_t* symbol = malloc(sizeof(symbol_t));
        symbol->name = "print";
        symbol->type = SYMBOL_FUNCTION;
        symbol->is_builtin = true;
        assert((symbol_table_insert(global_symbol_table, symbol) == INSERT_OK) && "Error when inserting builtin function");
    }
}

static void create_function_tables(node_t* function_declaration_node) {
    symbol_table_t* function_symtable = symbol_table_init();
    function_symtable->hashmap->backup = global_symbol_table->hashmap;

    node_t* identifier_node = function_declaration_node->children[0];
    node_t* parameter_list = function_declaration_node->children[1];

    for (size_t i = 0; i < da_size(parameter_list->children); ++i) {
        node_t* param_declaration = parameter_list->children[i];
        create_insert_variable_declaration(function_symtable, SYMBOL_PARAMETER, param_declaration);
    }

    symbol_t* function_symbol = malloc(sizeof(symbol_t));
    assert((identifier_node->type == IDENTIFIER) && "Expected identifier_node");
    function_symbol->name = identifier_node->data.identifier_str;
    function_symbol->type = SYMBOL_FUNCTION;
    function_symbol->node = function_declaration_node;
    function_symbol->function_symtable = function_symtable;
    function_symbol->is_builtin = false;
    if (symbol_table_insert(global_symbol_table, function_symbol) == INSERT_COLLISION) {
        // TODO: Note:
        fail_node(identifier_node, "Error: Redefinition of function '%s'", function_symbol->name);
    }

    bind_references(function_symtable, function_declaration_node->children[3]);
}

static void create_insert_variable_declaration(symbol_table_t* symtable, symbol_type_t symbol_type, node_t* declaration_node) {
    symbol_t* symbol = malloc(sizeof(symbol_t));
    assert((declaration_node->children[1]->type == IDENTIFIER) && "Expected identifier_node");
    symbol->name = declaration_node->children[1]->data.identifier_str;
    symbol->type = symbol_type;
    symbol->node = declaration_node->children[1];
    symbol->function_symtable = NULL;
    if (symbol_table_insert(symtable, symbol) == INSERT_COLLISION) {
        // TODO: Node:
        fail_node(declaration_node->children[1], "Error: Redefinition of variable '%s'", symbol->name);
    }
}

static void bind_references(symbol_table_t* local_symbols, node_t* node) {
    if (!node) return;

    if (node->type == BLOCK) {
        // Scope push
        symbol_hashmap_t* inner = symbol_hashmap_init();
        inner->backup = local_symbols->hashmap;
        local_symbols->hashmap = inner;

        for (size_t i = 0; i < da_size(node->children); ++i) {
            bind_references(local_symbols, node->children[i]);
        }

        // Scope pop
        symbol_hashmap_t* outer = local_symbols->hashmap->backup;
        symbol_hashmap_destroy(local_symbols->hashmap);
        local_symbols->hashmap = outer;
        return;
    }

    switch (node->type) {
        case RETURN_STATEMENT:
        case OPERATOR:
        case INTEGER_LITERAL:
        case STRING_LITERAL:
        case REAL_LITERAL:
        case PARENTHESIZED_EXPRESSION:
        case ASSIGNMENT_STATEMENT:
        case CAST_EXPRESSION:
        case TYPENAME:
        case IF_STATEMENT:
            {
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    bind_references(local_symbols, node->children[i]);
                }
            }
            break;
        case VARIABLE_DECLARATION:
            {
                // type, identifier, expression?
                if (da_size(node->children) == 3) {
                    bind_references(local_symbols, node->children[2]);
                }
                symbol_t* symbol = malloc(sizeof(symbol_t));
                assert((node->children[1]->type == IDENTIFIER) && "Expected identifier_node");
                symbol->name = node->children[1]->data.identifier_str;
                symbol->type = SYMBOL_LOCAL_VAR;
                symbol->node = node->children[1];
                symbol->function_symtable = local_symbols;
                if (symbol_table_insert(local_symbols, symbol) == INSERT_COLLISION) {
                    fail_node(node->children[1], "Error: Redefinition of variable '%s'", symbol->name);
                }
            }
            break;
        case IDENTIFIER:
            {
                // Assumes we didn't arrive here by declaration or function call
                char* identifier = node->data.identifier_str;
                symbol_t* symbol_definition = symbol_hashmap_lookup(local_symbols->hashmap, identifier);
                if (symbol_definition == NULL) {
                    fail_node(node, "Error: Unknown reference '%s'", identifier);
                }
                node->symbol = symbol_definition;
            }
            break;
        case FUNCTION_CALL:
            {
                node_t* identifier_node = node->children[0];
                symbol_t* symbol_definition = symbol_hashmap_lookup(local_symbols->hashmap, identifier_node->data.identifier_str);
                if (symbol_definition == NULL) {
                    fail_node(node->children[0], "Error: Unknown function reference '%s'", identifier_node->data.identifier_str);
                } 

                if (symbol_definition->type != SYMBOL_FUNCTION) {
                    // TODO: Note
                    fail_node(identifier_node, "Error: '%s' is not a function", symbol_definition->name);
                }

                identifier_node->symbol = symbol_definition;
                node_t* list_node = node->children[1];

                for (size_t i = 0; i < da_size(list_node->children); ++i) {
                    bind_references(local_symbols, list_node->children[i]);
                }
            }
            break;
        default:
            {
                fprintf(stderr, "bind_references: Unexpected node type: %s\n", NODE_TYPE_NAMES[node->type]);
                exit(EXIT_FAILURE);
            }
    }
}
