#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

#include "langc.h"
#include "interpret.h"
#include "tree.h"
#include "da.h"
#include "symbol.h"
#include "symbol_table.h"

typedef enum {
    TYPE_INT,
    TYPE_REAL,
    TYPE_STRING
} type_t;

static char* TYPE_NAMES[] = {
    "int",
    "real",
    "string"
};

typedef struct {
    symbol_t* symbol_definition;
    type_t type;
    union {
        long int_value;
        double real_value;
        char* string_value;
    } value;
} arg_bind_t;


static void* interpret_function(symbol_t*, arg_bind_t*);
static void* interpret_builtin_call(node_t*, arg_bind_t*);
static void interpret_print(node_t*, arg_bind_t*);
static void interpret_block(node_t*);
static arg_bind_t interpret_expression(node_t*);
static arg_bind_t* interpret_arg_list(node_t*);

#define fail(s, ...) do {\
            fprintf(stderr, "Error: "s"\n" __VA_OPT__(,) __VA_ARGS__);\
            exit(EXIT_FAILURE);\
        } while (0);

void interpret_root() {
    symbol_t* main_symbol = symbol_hashmap_lookup(global_symbol_table->hashmap, "main");
    if (main_symbol == NULL) {
        fail("No main function");
    }
    if (main_symbol->type != SYMBOL_FUNCTION) {
        fail("'main' must be a function");
    }
    // TODO: argc, argv, return value
    interpret_function(main_symbol, da_init(arg_bind_t));
}

static void* interpret_function(symbol_t* function_declaration, arg_bind_t* bind) {
    node_t* function_node = function_declaration->node;
    node_t* function_arg_list = function_node->children[1];

    // TODO: default args
    if (da_size(function_arg_list->children) != da_size(bind))
        fail("Function %s called with the wrong number of arguments", function_declaration->name);

    symbol_table_t* function_symtable = function_declaration->function_symtable;

    for (size_t i = 0; i < da_size(bind); ++i) {
        node_t* arg_declaration = function_arg_list->children[i];
        char* arg_identifier = arg_declaration->children[1]->data.identifier_str;
        symbol_t* symbol = symbol_hashmap_lookup(function_symtable->hashmap, arg_identifier);
        assert(symbol->type == SYMBOL_PARAMETER);
        char* typename_str = arg_declaration->children[0]->data.typename_str;

        arg_bind_t arg = bind[i];

        // TODO: better typechecking framework
        if (strcmp(typename_str, "int") == 0) {
            if (arg.type != TYPE_INT)
                fail("Argument '%s' has the wrong type (should be int)", symbol->name);
            symbol->data.int_value = arg.value.int_value;
        } else if (strcmp(typename_str, "real") == 0) {
            if (arg.type != TYPE_REAL)
                fail("Argument '%s' has the wrong type (should be real)", symbol->name);

            symbol->data.real_value = arg.value.real_value;
        } else if (strcmp(typename_str, "string") == 0) {
            if (arg.type != TYPE_STRING)
                fail("Argument '%s' has the wrong type (should be real)", symbol->name);

            symbol->data.string_value = arg.value.string_value;
        } else {
            fail("Unknown typename '%s'", typename_str);
        }
    }

    // TODO: return type etc.
    interpret_block(function_node->children[3]);

    return NULL;
}

static void* interpret_builtin_call(node_t* function_call_node, arg_bind_t* args) {
    if (strcmp(function_call_node->children[0]->symbol->name, "print") == 0) {
        interpret_print(function_call_node, args);
        return NULL;
    } else {
        fail("Unknown builtin '%s'", function_call_node->children[0]->symbol->name);
    }
    assert(false && "Unreachable");
}

static void interpret_print(node_t* print_node, arg_bind_t* args) {
    for (size_t i = 0; i < da_size(args); ++i) {
        if (i > 0)printf(" ");
        arg_bind_t arg = args[i];
        if (arg.type == TYPE_STRING) {
            printf("%s", arg.value.string_value);
        } else if (arg.type == TYPE_REAL) {
            printf("%f", arg.value.real_value);
        } else if (arg.type == TYPE_INT) {
            printf("%ld", arg.value.int_value);
        } else {
            assert(false && "Invalid type in print");
        }
    }
    puts("");
}

static void interpret_block(node_t* block_node) {
    for (size_t i = 0; i < da_size(block_node->children); ++i) {
        node_t* statement_node = block_node->children[i];

        switch (statement_node->type) {
            case FUNCTION_CALL:
                {
                    symbol_t* function_symbol = statement_node->children[0]->symbol;
                    arg_bind_t* bind_list = interpret_arg_list(statement_node->children[1]);
                    assert((function_symbol != NULL) && "Expected function symbol to be bound.");
                    if (function_symbol->is_builtin) {
                        interpret_builtin_call(statement_node, bind_list);
                    }
                }
                break;
            default:
                {
                    fail("Not implemented statement type: %s", NODE_TYPE_NAMES[statement_node->type]);
                }
        }
    }
}

static arg_bind_t interpret_expression(node_t* expression_node) {
    switch (expression_node->type) {
        case STRING_LITERAL:
            {
                return (arg_bind_t){
                    .type = TYPE_STRING,
                    .value.string_value = expression_node->data.string_literal_value,
                    .symbol_definition = NULL
                };
            }
        case INTEGER_LITERAL:
            {
                return (arg_bind_t){
                    .type = TYPE_INT,
                    .value.int_value = expression_node->data.int_literal_value,
                    .symbol_definition = NULL
                };
            }
        case OPERATOR:
            {
                if (da_size(expression_node->children) == 2) {
                    arg_bind_t lhs = interpret_expression(expression_node->children[0]);
                    arg_bind_t rhs = interpret_expression(expression_node->children[1]);
                    if (strcmp(expression_node->data.operator_str, "+") == 0) {
                        if (lhs.type == TYPE_STRING && rhs.type == TYPE_STRING) {
                            assert(false && "Not implemented: string concatenation");
                        }

                        if (lhs.type == TYPE_STRING || rhs.type == TYPE_STRING) {
                            fail("Cannot concatenate %s and %s", TYPE_NAMES[lhs.type], TYPE_NAMES[rhs.type]);
                        }

                        if (lhs.type == TYPE_INT && rhs.type == TYPE_INT) {
                            return (arg_bind_t){
                                .type = TYPE_INT,
                                .value.int_value = lhs.value.int_value + rhs.value.int_value,
                                .symbol_definition = NULL
                            };
                        }
                        assert(false && "Type handling");
                    }
                } else {
                }
            }
            break;
        default:
            {
                fail("Not implemented node type %s", NODE_TYPE_NAMES[expression_node->type]);
            }
    }
}

static arg_bind_t* interpret_arg_list(node_t* expression_node) {
    arg_bind_t* res = da_init(arg_bind_t);
    for (size_t i = 0; i < da_size(expression_node->children); ++i) {
        arg_bind_t value = interpret_expression(expression_node->children[i]);
        da_append(&res, value);
    }
    return res;
}
