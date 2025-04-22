#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "type.h"
#include "da.h"
#include "langc.h"
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
    "string"
};

static void register_type_node(node_t*);
static type_info_t* get_builtin_function_type(symbol_t*);
static bool types_equivalent(type_info_t* type_a, type_info_t* type_b);
static bool can_cast(type_info_t* type_dst, type_info_t* type_src);
static type_info_t* create_basic(basic_type_t basic_type);
static type_info_t* create_type_function();
static type_info_t* create_type_array(type_info_t*, size_t);
static type_tuple_t* create_tuple();


void register_types() {
    for (size_t i = 0; i < da_size(root->children); ++i) {
        register_type_node(root->children[i]);
    }
}

node_t* current_function_node = NULL;
static void register_type_node(node_t* node) {
    if (node->type_info != NULL) return;

    switch (node->type) {
        case VARIABLE_DECLARATION:
            {
                // Typename node
                register_type_node(node->children[0]);
                if (da_size(node->children) == 3) {
                    // type identifier := expression
                    // Check if expression has same type as declared
                    register_type_node(node->children[2]);

                    if (!types_equivalent(node->children[0]->type_info, node->children[2]->type_info)) {
                        fprintf(stderr, "Cannot assign %s of type ", node->children[1]->data.identifier_str);
                        type_print(stderr, node->children[0]->type_info);
                        fprintf(stderr, " to expression of type ");
                        type_print(stderr, node->children[2]->type_info);
                        fprintf(stderr, "\n");
                        exit(EXIT_FAILURE);
                    }
                }
                node->type_info = node->children[0]->type_info;
                node->children[1]->type_info = node->type_info;
                return;
            }
            break;
        case TYPENAME:
            {
                char* type_str = node->data.typename_str;
                if (strcmp(type_str, "int") == 0) {
                    node->type_info = create_basic(TYPE_INT);
                } else if (strcmp(type_str, "real") == 0) {
                    node->type_info = create_basic(TYPE_REAL);
                } else if (strcmp(type_str, "void") == 0) {
                    node->type_info = create_basic(TYPE_VOID);
                } else if (strcmp(type_str, "bool") == 0) {
                    node->type_info = create_basic(TYPE_BOOL);
                } else if (strcmp(type_str, "char") == 0) {
                    node->type_info = create_basic(TYPE_CHAR);
                } else if (strcmp(type_str, "string") == 0) {
                    node->type_info = create_basic(TYPE_STRING);
                } else {
                    fail("Unhandled type name %s", type_str);
                }

                // Could be array type
                type_info_t* result_type = node->type_info;
                for (int i = (int)da_size(node->children) - 1; i >= 0; --i) {
                    node_t* count_node = node->children[i];
                    assert(count_node->type == INTEGER_LITERAL && "Unexpected type child");

                    size_t count = count_node->data.int_literal_value;

                    result_type = create_type_array(result_type, count);
                }
                node->type_info = result_type;

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
        case FUNCTION_DECLARATION:
            {
                // Return type
                register_type_node(node->children[2]);

                node_t* old_function_node = current_function_node;
                current_function_node = node;

                // Is it sketchy to save before everything has been computed
                // Infinite loop if not, i think
                node->type_info = create_type_function();
                node->type_info->info.info_function->return_type = node->children[2]->type_info;

                // Declarations in arg_list
                for (size_t i = 0; i < da_size(node->children[1]->children); ++i) {
                    register_type_node(node->children[1]->children[i]);
                    da_append(&node->type_info->info.info_function->arg_types->elems, node->children[1]->children[i]->type_info);
                }

                // Block
                register_type_node(node->children[3]);

                // TODO: Check return statement type

                current_function_node = old_function_node;

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
                if (current_function_node == NULL) {
                    fail("Return statement not allowed outside function");
                }
                type_info_t* required_return_type = current_function_node->type_info->info.info_function->return_type;
                if (!types_equivalent(node->type_info, required_return_type)) {
                    fprintf(stderr, "Function '%s', with return type '", current_function_node->children[0]->data.identifier_str);
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
                    fprintf(stderr, "' to '%s', which has type '", node->children[0]->data.identifier_str);
                    type_print(stderr, node->children[0]->type_info);
                    fprintf(stderr, "'\n");
                    exit(EXIT_FAILURE);
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
        default:
            fail("Not implemented type check: %s", NODE_TYPE_NAMES[node->type]);
    }
}

static type_info_t* get_builtin_function_type(symbol_t* function_symbol) {
    if (strcmp(function_symbol->name, "print") == 0) {
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

        if (array_a->count != array_b->count) return false;
        return types_equivalent(array_a->subtype, array_b->subtype);
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

static type_info_t* create_type_array(type_info_t* subtype, size_t count) {
    type_info_t *type_info = malloc(sizeof(type_info_t));
    type_info->type_class = TC_ARRAY;
    type_info->info.info_array = malloc(sizeof(type_array_t));
    type_info->info.info_array->count = count;
    type_info->info.info_array->subtype = subtype;
    return type_info;
}

static type_tuple_t* create_tuple() {
    type_tuple_t* tuple = malloc(sizeof(type_tuple_t));
    tuple->elems = da_init(type_info_t*);
    return tuple;
}

void type_print(FILE* stream, type_info_t* type) {
    switch (type->type_class) {
        case TC_BASIC:
            fprintf(stream, "%s", BASIC_TYPE_NAMES[type->info.info_basic]);
            break;
        case TC_ARRAY:
            {
                fprintf(stream, "array(%zu, ", type->info.info_array->count);
                type_print(stream, type->info.info_array->subtype);
                fprintf(stream, ")");
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
    }
}
