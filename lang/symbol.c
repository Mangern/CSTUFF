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
#include "type.h"


static void insert_builtin_functions();
static void create_function_tables(node_t*);
static void create_insert_variable_declaration(symbol_table_t*, symbol_type_t, node_t*);
static void bind_references(symbol_table_t*, node_t*);
static symbol_t* symbol_resolve_scope(symbol_table_t*, node_t*);

char* SYMBOL_TYPE_NAMES[] = {
    "GLOBAL_VAR",
    "FUNCTION",
    "PARAMETER",
    "LOCAL_VAR"
};

symbol_table_t* global_symbol_table;
char** global_string_list = 0;

void create_symbol_tables() {

    global_symbol_table = symbol_table_init();

    insert_builtin_functions();

    for (size_t i = 0; i < da_size(root->children); ++i) {
        node_t* node = root->children[i];
        if (node->type == DECLARATION) {
            node_t* typenode = node->children[1];
            if (typenode->type == TYPE) {
                if (typenode->data.type_class == TC_FUNCTION) {
                    create_function_tables(node);
                } else {
                    create_insert_variable_declaration(global_symbol_table, SYMBOL_GLOBAL_VAR, node);
                }
            } else if (typenode->type == BLOCK) {
                fail_node(node, "Cannot infer type of %s", node->children[0]->data.identifier_str);
            } else {
                // expression
                create_insert_variable_declaration(global_symbol_table, SYMBOL_GLOBAL_VAR, node);
            }
        } else {
            assert(false && "Unexpected node type in global statement list");
        }
    }
}

static void insert_builtin_functions() {
    {
        symbol_t* symbol = malloc(sizeof(symbol_t));
        symbol->name = "println";
        symbol->type = SYMBOL_FUNCTION;
        symbol->is_builtin = true;
        assert((symbol_table_insert(global_symbol_table, symbol) == INSERT_OK) && "Error when inserting builtin function");
    }

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
    node_t* func_type_node = function_declaration_node->children[1];

    node_t* param_declaration_list = func_type_node->children[0];

    for (size_t i = 0; i < da_size(param_declaration_list->children); ++i) {
        node_t* param_declaration = param_declaration_list->children[i];
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

    bind_references(function_symtable, function_declaration_node->children[2]);
}

static void create_insert_variable_declaration(symbol_table_t* symtable, symbol_type_t symbol_type, node_t* declaration_node) {
    symbol_t* symbol = malloc(sizeof(symbol_t));
    node_t* identifier_node = declaration_node->children[0];
    assert((identifier_node->type == IDENTIFIER) && "Expected identifier_node");
    symbol->name = identifier_node->data.identifier_str;
    symbol->type = symbol_type;
    symbol->node = identifier_node;
    symbol->function_symtable = NULL;
    symbol->node->symbol = symbol;
    if (symbol_table_insert(symtable, symbol) == INSERT_COLLISION) {
        // TODO: Node:
        fail_node(identifier_node, "Error: Redefinition of variable '%s'", symbol->name);
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
        case REAL_LITERAL:
        case PARENTHESIZED_EXPRESSION:
        case ASSIGNMENT_STATEMENT:
        case CAST_EXPRESSION:
        case TYPE:
        case IF_STATEMENT:
        case WHILE_STATEMENT:
        case ARRAY_INDEXING:
        case LIST:
        case BOOL_LITERAL:
        case BREAK_STATEMENT:
            {
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    bind_references(local_symbols, node->children[i]);
                }
            }
            break;
        case DECLARATION:
            {
                // identifier, type, expression?
                if (da_size(node->children) == 3) {
                    bind_references(local_symbols, node->children[2]);
                } else if (da_size(node->children) == 2 && node->children[1]->type != TYPE) {
                    bind_references(local_symbols, node->children[1]);
                }
                symbol_t* symbol = malloc(sizeof(symbol_t));
                node_t* identifier = node->children[0];
                assert((identifier->type == IDENTIFIER) && "Expected identifier_node");
                symbol->name = identifier->data.identifier_str;
                symbol->type = SYMBOL_LOCAL_VAR;
                symbol->node = identifier;
                symbol->function_symtable = local_symbols;
                symbol->node->symbol = symbol;
                if (symbol_table_insert(local_symbols, symbol) == INSERT_COLLISION) {
                    fail_node(identifier, "Error: Redefinition of variable '%s'", symbol->name);
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
                node_t* lhs_node = node->children[0];

                symbol_t* symbol_definition = 0;
                if (lhs_node->type == IDENTIFIER) {
                    symbol_definition = symbol_hashmap_lookup(local_symbols->hashmap, lhs_node->data.identifier_str);
                } else {
                    symbol_definition = symbol_resolve_scope(local_symbols, lhs_node);
                }
                if (symbol_definition == NULL) {
                    fail_node(node->children[0], "Error: Unknown function reference '%s'", lhs_node->data.identifier_str);
                } 

                if (symbol_definition->type != SYMBOL_FUNCTION) {
                    // TODO: Note
                    fail_node(lhs_node, "Error: '%s' is not a function", symbol_definition->name);
                }

                lhs_node->symbol = symbol_definition;
                node_t* list_node = node->children[1];

                for (size_t i = 0; i < da_size(list_node->children); ++i) {
                    bind_references(local_symbols, list_node->children[i]);
                }
            }
            break;
        case STRING_LITERAL:
            {
                // Store string data in string table instead
                size_t idx = da_size(global_string_list);
                da_append(global_string_list, node->data.string_literal_value);
                node->data.string_literal_idx = idx;
            }
            break;
        default:
            {
                fprintf(stderr, "bind_references: Unexpected node type: %s\n", NODE_TYPE_NAMES[node->type]);
                exit(EXIT_FAILURE);
            }
    }
}

// resolve scope resolution
static symbol_t* symbol_resolve_scope(symbol_table_t* local_symbols, node_t* scope_resolution_node) {

    node_t* lhs_node = scope_resolution_node->children[0];

    if (lhs_node->type == IDENTIFIER) {
        symbol_t* symbol_definition = symbol_hashmap_lookup(local_symbols->hashmap, lhs_node->data.identifier_str);
        (void)symbol_definition;
    }

    assert(false && "Not done");
}
