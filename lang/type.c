#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "type.h"
#include "da.h"
#include "fail.h"
#include "langc.h"
#include "lex.h"
#include "tree.h"

#define fail(s, ...) do {\
            fprintf(stderr, "Error: "s"\n" __VA_OPT__(,) __VA_ARGS__);\
            exit(EXIT_FAILURE);\
        } while (0);

char* BASIC_TYPE_NAMES[] = {
    "void",
    "int",
    "real",
    "char",
    "bool",
    "string",
    "size"
};

static void register_type_node(node_t*);
static type_info_t* get_builtin_function_type(symbol_t*);
static bool types_equivalent(type_info_t* type_a, type_info_t* type_b);
static bool can_cast(type_info_t* type_dst, type_info_t* type_src);
static type_info_t* create_basic(basic_type_t basic_type);
static type_info_t* create_type_function();
static type_info_t* create_type_array(type_info_t*, node_t*);
static type_info_t* create_type_pointer(type_info_t*);
static type_tuple_t* create_tuple();
static basic_type_t is_basic_type(const char* identifier_str);


void register_types() {
    for (size_t i = 0; i < da_size(root->children); ++i) {
        register_type_node(root->children[i]);
    }
}

// Points to the DECLARATION of the curr function
node_t* current_function_type_node = NULL;

static void register_type_node(node_t* node) {
    if (node->type_info != NULL) return;

    switch (node->type) {
        case DECLARATION:
            {
                // Type node
                register_type_node(node->children[1]);
                node_t* old_function_node = current_function_type_node;

                if (da_size(node->children) == 3) {
                    bool is_function = node->children[1]->type == TYPE 
                        && node->children[1]->data.type_class == TC_FUNCTION;
                    if (is_function) {
                        old_function_node = current_function_type_node;
                        current_function_type_node = node->children[1];
                    }

                    // we need to save it here because if we have a 
                    // recursive function, we need to 'memoize' the type info
                    // so that we don't recurse infinitely

                    assert(node->children[1]->type == TYPE 
                            && "When declaration has three children I expect middle one to be type");

                    node->type_info = node->children[1]->type_info;
                    node->children[0]->type_info = node->type_info;

                    // identifier: type = expression
                    // Check if expression has same type as declared
                    register_type_node(node->children[2]);

                    if (!is_function && !types_equivalent(node->children[1]->type_info, node->children[2]->type_info)) {
                        // TODO: better error
                        fprintf(stderr, "Cannot assign %s of type ", node->children[0]->data.identifier_str);
                        type_print(stderr, node->children[1]->type_info);
                        fprintf(stderr, " to expression of type ");
                        type_print(stderr, node->children[2]->type_info);
                        fprintf(stderr, "\n");
                        exit(EXIT_FAILURE);
                    }
                }

                if (node->children[1]->type != TYPE) {
                    if (node->children[1]->type_info == NULL) {
                        fail_node(node->children[1], "Could not infer type of expression.")
                    }
                }
                node->type_info = node->children[1]->type_info;
                node->children[0]->type_info = node->type_info;

                current_function_type_node = old_function_node;
                return;
            }
            break;
        case TYPE:
            {
                if (node->data.type_class == TC_FUNCTION) {
                    for (size_t i = 0; i < da_size(node->children); ++i) {
                        register_type_node(node->children[i]);
                    }

                    node->type_info = create_type_function();
                    node->type_info->info.info_function->return_type 
                        = node->children[1]->type_info;

                    for (size_t i = 0; i < da_size(node->children[0]->children); ++i) {
                        da_append(
                            node->type_info->info.info_function->arg_types->elems,
                            node->children[0]->children[i]->type_info
                        );
                    }
                    // TODO: arg types
                    return;
                } else if (node->data.type_class == TC_UNKNOWN) {
                    assert(da_size(node->children) == 1);

                    node_t* identifier = node->children[0];
                    assert(identifier->type == IDENTIFIER);

                    basic_type_t basic_type = is_basic_type(identifier->data.identifier_str);
                    if ((basic_type >= 0)) {
                        node->type_info = create_basic(basic_type);
                    } else {
                        fail_node(identifier, "Unknown type %s", identifier->data.identifier_str);
                    }
                } else if (node->data.type_class == TC_ARRAY) {
                    // first child: (sybtype, second: list)
                    register_type_node(node->children[0]);
                    register_type_node(node->children[1]);
                    node->type_info = create_type_array(node->children[0]->type_info, node->children[1]);
                } else if (node->data.type_class == TC_POINTER) {
                    // only child: subtype
                    register_type_node(node->children[0]);
                    node->type_info = create_type_pointer(node->children[0]->type_info);
                } else {
                    assert(false && "Unhandled type class from parsing stage");
                }

                return;
            }
            break;
        case INTEGER_LITERAL:
            {
                node->type_info = create_basic(TYPE_INT);
                return;
            }
            break;
        case REAL_LITERAL:
            {
                node->type_info = create_basic(TYPE_REAL);
                return;
            }
            break;
        case STRING_LITERAL:
            {
                node->type_info = create_basic(TYPE_STRING);
                return;
            }
            break;
        case BOOL_LITERAL:
            {
                node->type_info = create_basic(TYPE_BOOL);
                return;
            }
            break;
        case DECLARATION_LIST:
            {
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    register_type_node(node->children[i]);
                }
                // TODO
                node->type_info = create_basic(TYPE_VOID);
                return;
            }
            break;
        case BLOCK:
            {
                // Should the block itself have a return type? Last statment in block?
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    register_type_node(node->children[i]);
                }
            }
            break;
        case RETURN_STATEMENT:
            {
                // Type of return statement is the type of the returned expression
                if (da_size(node->children) > 0) {
                    register_type_node(node->children[0]);
                    node->type_info = node->children[0]->type_info;
                } else {
                    node->type_info = create_basic(TYPE_VOID);
                }
                if (current_function_type_node == NULL) {
                    fail("Return statement not allowed outside function");
                }
                type_info_t* required_return_type = current_function_type_node->type_info->info.info_function->return_type;
                if (!types_equivalent(node->type_info, required_return_type)) {
                    fprintf(stderr, "Function '%s', with return type '", current_function_type_node->children[0]->data.identifier_str);
                    type_print(stderr, required_return_type);
                    fprintf(stderr, "', cannot return '");
                    type_print(stderr, node->type_info);
                    fprintf(stderr, "'\n");
                    exit(EXIT_FAILURE);
                }
                return;
            }
            break;
        case OPERATOR:
            {
                register_type_node(node->children[0]);
                if (da_size(node->children) == 2) {
                    register_type_node(node->children[1]);
                }

                // TODO: Automatic cast?
                char* opstr;
                switch (node->data.operator) {
                    case BINARY_ADD:
                    case BINARY_SUB:
                    case BINARY_MUL:
                    case BINARY_DIV:
                    case BINARY_MOD:
                        {
                            switch(node->data.operator) {
                                case BINARY_ADD:
                                    opstr = "add";
                                    break;
                                case BINARY_SUB:
                                    opstr = "subtract";
                                    break;
                                case BINARY_MUL:
                                    opstr = "multiply";
                                    break;
                                case BINARY_DIV:
                                    opstr = "divide";
                                    break;
                                case BINARY_MOD:
                                    opstr = "mod";
                                    break;
                                default:
                                    assert(false);
                            }

                            if (!types_equivalent(node->children[0]->type_info, node->children[1]->type_info)) {
                                fprintf(stderr, "Cannot %s ", opstr);
                                type_print(stderr, node->children[0]->type_info);
                                fprintf(stderr, " and ");
                                type_print(stderr, node->children[1]->type_info);
                                fprintf(stderr, "\n");
                                exit(EXIT_FAILURE);
                            }

                            node->type_info = node->children[0]->type_info;
                            return;
                        }
                        break;
                    case BINARY_GT:
                    case BINARY_LT:
                    case BINARY_LEQ:
                    case BINARY_GEQ:
                    case BINARY_EQ:
                    case BINARY_NEQ:
                        {
                            if (!types_equivalent(node->children[0]->type_info, node->children[1]->type_info)) {
                                fprintf(stderr, "Cannot combine ");
                                type_print(stderr, node->children[0]->type_info);
                                fprintf(stderr, " and ");
                                type_print(stderr, node->children[1]->type_info);
                                fprintf(stderr, "\n");
                                exit(EXIT_FAILURE);
                            }

                            node->type_info = create_basic(TYPE_BOOL);
                            return;
                        }
                        break;
                    case UNARY_SUB:
                    case UNARY_NEG:
                        {
                            node->type_info = node->children[0]->type_info;
                            return;
                        }
                        break;
                    case UNARY_DEREF:
                        {
                            node_t* inner = node->children[0];
                            if (inner->type_info->type_class != TC_POINTER) {
                                fail_node(node, "Cannot dereference this");
                            }

                            node->type_info = inner->type_info->info.info_pointer->inner;
                            return;
                        }
                        break;
                    case UNARY_STAR:
                        {
                            node_t* inner = node->children[0];
                            node->type_info = create_type_pointer(inner->type_info);
                            return;
                        }
                    default:
                        {
                            fail("Not implemented operator type check: %s", OPERATOR_TYPE_NAMES[node->data.operator]);
                        }
                        break;
                }
            }
            break;
        case IDENTIFIER:
            {
                if (node->symbol == NULL)
                    fail_node(node, "bind ref fail");

                assert(node->symbol != NULL && "Bind references has not succeeded");
                node_t* symbol_definition_node = node->symbol->node;
                assert(symbol_definition_node->type_info != NULL);
                node->type_info = symbol_definition_node->type_info;
            }
            break;
        case FUNCTION_CALL:
            {
                // Type of function call: Return type of the called function
                // Register args
                register_type_node(node->children[1]);

                assert(node->children[0]->symbol != NULL);
                symbol_t *function_symbol = node->children[0]->symbol;
                node_t *symbol_definition_node = function_symbol->node;

                if (function_symbol->is_builtin) {
                    // TODO: check signature of builtin
                    node->type_info = get_builtin_function_type(function_symbol);
                    return;
                } 

                if (symbol_definition_node->type_info == NULL) {
                    register_type_node(symbol_definition_node);
                }
                assert(symbol_definition_node->type_info != NULL);
                assert(symbol_definition_node->type_info->type_class == TC_FUNCTION);

                type_function_t* info_function = symbol_definition_node->type_info->info.info_function;

                node_t* args_list= node->children[1];

                if (da_size(args_list->children) != da_size(info_function->arg_types->elems)) {
                    fail("Function %s requires exactly %zu arguments, but was called with %zu\n",
                        function_symbol->name,
                        da_size(info_function->arg_types->elems),
                        da_size(args_list->children)
                    );
                }

                for (size_t i = 0; i < da_size(args_list->children); ++i) {
                    if (!types_equivalent(args_list->children[i]->type_info, info_function->arg_types->elems[i])) {
                        fprintf(stderr, "Argument %zu of function %s has the wrong type. Required: '", i+1, function_symbol->name);
                        type_print(stderr, info_function->arg_types->elems[i]);
                        fprintf(stderr, "', Got: '");
                        type_print(stderr, args_list->children[i]->type_info);
                        fprintf(stderr, "'\n");
                        exit(EXIT_FAILURE);
                    }
                }

                node->type_info = info_function->return_type;
            }
            break;
        case PARENTHESIZED_EXPRESSION:
            {
                register_type_node(node->children[0]);
                node->type_info = node->children[0]->type_info;
            }
            break;
        case ASSIGNMENT_STATEMENT:
            {
                // lhs := rhs
                register_type_node(node->children[0]);
                register_type_node(node->children[1]);

                if (!types_equivalent(node->children[0]->type_info, node->children[1]->type_info)) {
                    fprintf(stderr, "Cannot assign expression of type '");
                    type_print(stderr, node->children[1]->type_info);
                    fprintf(stderr, "' to element of type '");
                    type_print(stderr, node->children[0]->type_info);
                    fprintf(stderr, "'\n");
                    fail_node(node, "%s", "");
                }

                // Should it be void or type of assignment?
                node->type_info = create_basic(TYPE_VOID);
            }
            break;
        case CAST_EXPRESSION:
            {
                register_type_node(node->children[0]);
                register_type_node(node->children[1]);

                if (!can_cast(node->children[1]->type_info, node->children[0]->type_info)) {
                    fprintf(stderr, "Cannot cast '");
                    type_print(stderr, node->children[1]->type_info);
                    fprintf(stderr, "' to '");
                    type_print(stderr, node->children[0]->type_info);
                    fprintf(stderr, "'\n");
                    exit(EXIT_FAILURE);
                }

                node->type_info = node->children[0]->type_info;
            }
            break;
        case LIST:
            {
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    register_type_node(node->children[i]);
                }
                // TODO: tuple type here in some cases?
                node->type_info = create_basic(TYPE_VOID);
            }
            break;
        case IF_STATEMENT:
            {
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    register_type_node(node->children[i]);
                }

                // first child has to have boolean type
                if (node->children[0]->type_info->type_class != TC_BASIC
                 || node->children[0]->type_info->info.info_basic != TYPE_BOOL) {
                    fail("Condition of if statement must be boolean.");
                }
                node->type_info = create_basic(TYPE_VOID);
            }
            break;
        case ARRAY_INDEXING:
            {
                register_type_node(node->children[0]);
                register_type_node(node->children[1]);

                type_info_t* array_type = node->children[0]->type_info;
                if (array_type->type_class != TC_ARRAY) {
                    fail_node(node, "Attempt to index non-array");
                }

                node_t* dim_list = node->children[1];

                if (da_size(dim_list->children) != da_size(array_type->info.info_array->dims)) {
                    fail_node(node, "Wrong number of dimensions");
                }

                for (size_t i = 0; i < da_size(dim_list->children); ++i) {
                    if (dim_list->children[i]->type_info->type_class != TC_BASIC || dim_list->children[i]->type_info->info.info_basic != TYPE_INT) {
                        fail_node(dim_list->children[i], "Array indices must be integers");
                    }
                }

                node->type_info = node->children[0]->type_info->info.info_array->subtype;
            }
            break;
        case WHILE_STATEMENT:
            {
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    register_type_node(node->children[i]);
                }

                if (node->children[0]->type_info->type_class != TC_BASIC
                 || node->children[0]->type_info->info.info_basic != TYPE_BOOL) {
                    fail("Condition of while statement must be boolean.");
                }
                node->type_info = create_basic(TYPE_VOID);
            }
            break;
        case BREAK_STATEMENT:
            {
                node->type_info = create_basic(TYPE_VOID);
            }
            break;
        default:
            fail("Not implemented type check: %s", NODE_TYPE_NAMES[node->type]);
    }
}

static type_info_t* get_builtin_function_type(symbol_t* function_symbol) {
    if (strcmp(function_symbol->name, "println") == 0) {
        type_info_t* type_info = create_type_function();
        type_info->info.info_function->return_type = create_basic(TYPE_VOID);
        return type_info;
    } else if (strcmp(function_symbol->name, "print") == 0) {
        type_info_t* type_info = create_type_function();
        type_info->info.info_function->return_type = create_basic(TYPE_VOID);
        return type_info;
    } else {
        fail("Not implemented builtin function type: %s", function_symbol->name);
    }
    assert(false);
}

static bool types_equivalent(type_info_t* type_a, type_info_t* type_b) {
    if (type_a->type_class != type_b->type_class) return false;

    type_class_t type_class = type_a->type_class;
    if (type_class == TC_BASIC) {
        return type_a->info.info_basic == type_b->info.info_basic;
    } else if (type_class == TC_ARRAY) {
        type_array_t* array_a = type_a->info.info_array;
        type_array_t* array_b = type_b->info.info_array;

        if (da_size(array_a->dims) != da_size(array_b->dims)) return false;
        for (size_t i = 0; i < da_size(array_a->dims); ++i) {
            if (array_a->dims[i] != array_b->dims[i]) return false;
        }
        return types_equivalent(array_a->subtype, array_b->subtype);
    } else if (type_class == TC_POINTER) {
        return types_equivalent(
            type_a->info.info_pointer->inner,
            type_b->info.info_pointer->inner
        );
    } else {
        fprintf(stderr, "Not implemented type class equivalency:\n");
        type_print(stderr, type_a);
        fprintf(stderr, "\n");
        exit(EXIT_FAILURE);
    }
}

static bool can_cast(type_info_t* type_dst, type_info_t* type_src) {
    if (types_equivalent(type_dst, type_src))
        return true;
    if (type_dst->type_class != TC_BASIC)
        return false;
    if (type_src->type_class != TC_BASIC)
        return false;

    if (type_dst->info.info_basic == TYPE_STRING || type_src->info.info_basic == TYPE_STRING)
        return false;

    // TODO: Actual casting logic.
    return true;
}

static type_info_t* create_basic(basic_type_t basic_type) {
    type_info_t* type_info = malloc(sizeof(type_info_t));
    type_info->type_class = TC_BASIC;
    type_info->info.info_basic = basic_type;
    return type_info;
}

static type_info_t* create_type_function() {
    type_info_t *type_info = malloc(sizeof(type_info_t));
    type_info->type_class = TC_FUNCTION;

    type_info->info.info_function = malloc(sizeof(type_function_t));
    type_info->info.info_function->arg_types = create_tuple();
    return type_info;
}

static type_info_t* create_type_array(type_info_t* subtype, node_t* dim_list_node) {
    type_info_t *type_info = malloc(sizeof(type_info_t));
    type_info->type_class = TC_ARRAY;
    type_info->info.info_array = malloc(sizeof(type_array_t));
    type_info->info.info_array->dims = 0;
    for (size_t i = 0; i < da_size(dim_list_node->children); ++i) {
        da_append(type_info->info.info_array->dims, (size_t)dim_list_node->children[i]->data.int_literal_value);
    }
    type_info->info.info_array->subtype = subtype;
    return type_info;
}

static type_info_t* create_type_pointer(type_info_t* subtype) {
    type_info_t *type_info = malloc(sizeof(type_info_t));
    type_info->type_class = TC_POINTER;
    type_info->info.info_pointer = malloc(sizeof(type_pointer_t));
    type_info->info.info_pointer->inner = subtype;
    return type_info;
}

static type_tuple_t* create_tuple() {
    type_tuple_t* tuple = malloc(sizeof(type_tuple_t));
    tuple->elems = 0;
    return tuple;
}

void type_print(FILE* stream, type_info_t* type) {
    switch (type->type_class) {
        case TC_BASIC:
            fprintf(stream, "%s", BASIC_TYPE_NAMES[type->info.info_basic]);
            break;
        case TC_ARRAY:
            {
                type_print(stream, type->info.info_array->subtype);
                fprintf(stream, "[");
                for (size_t i = 0; i < da_size(type->info.info_array->dims); ++i) {
                    if (i > 0)fprintf(stream, ", ");
                    fprintf(stream, "%zu", type->info.info_array->dims[i]);
                }
                fprintf(stream, "]");
            }
            break;
        case TC_STRUCT:
            {
                fprintf(stream, "{ ");
                for (size_t i = 0; i < da_size(type->info.info_struct->fields); ++i) {
                    if (i > 0)
                        fprintf(stream, ", ");

                    type_named_t* field = type->info.info_struct->fields[i];
                    type_print(stream, field->type);
                    fprintf(stream, " %s", field->name);
                }
                fprintf(stream, " }");
            }
            break;
        case TC_TUPLE:
            {
                fprintf(stream, "( ");
                for (size_t i = 0; i < da_size(type->info.info_tuple->elems); ++i) {
                    if (i > 0)fprintf(stream, ", ");
                    type_print(stream, type->info.info_tuple->elems[i]);
                }
                fprintf(stream, " )");
            }
            break;
        case TC_FUNCTION:
            {
                fprintf(stream, "( ");
                for (size_t i = 0; i < da_size(type->info.info_function->arg_types->elems); ++i) {
                    if (i > 0)fprintf(stream, ", ");
                    type_print(stream, type->info.info_function->arg_types->elems[i]);
                }
                fprintf(stream, " ) -> ");
                type_print(stream, type->info.info_function->return_type);
            }
            break;
        case TC_POINTER:
            {
                fprintf(stream, "*");
                type_print(stream, type->info.info_pointer->inner);
            }
            break;
        case TC_UNKNOWN:
            {
                fprintf(stream, "unknown");
                break;
            }
    }
}

static basic_type_t is_basic_type(const char* identifier_str) {
    if (strcmp(identifier_str, "int") == 0) {
        return TYPE_INT;
    } else if (strcmp(identifier_str, "real") == 0) {
        return TYPE_REAL;
    } else if (strcmp(identifier_str, "void") == 0) {
        return TYPE_VOID;
    } else if (strcmp(identifier_str, "bool") == 0) {
        return TYPE_BOOL;
    } else if (strcmp(identifier_str, "char") == 0) {
        return TYPE_CHAR;
    } else if (strcmp(identifier_str, "string") == 0) {
        return TYPE_STRING;
    }
    return -1;
}

size_t type_sizeof(type_info_t* type) {
    // TODO: may be benefitial to store alongside the type info?

    if (type->type_class == TC_BASIC) {
        return 8;
    } else if (type->type_class == TC_ARRAY) {
        size_t total_size = 1;
        for (size_t j = 0; j < da_size(type->info.info_array->dims); ++j) {
            total_size *= type->info.info_array->dims[j];
        }
        total_size *= 8;
        return total_size;
    } else if (type->type_class == TC_POINTER) {
        return 8;
    } else {
        assert(false && "Not implemented");
    }
}
