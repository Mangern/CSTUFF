#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "tac.h"
#include "da.h"
#include "fail.h"
#include "symbol.h"
#include "symbol_table.h"
#include "tree.h"
#include "type.h"

function_code_t* function_codes;
addr_t* addr_list;

static char* TAC_INSTRUCTION_NAMES[] = {
    "NOP",
    "RETURN", 
    "BINARY_ADD",
    "BINARY_SUB",
    "BINARY_MUL",
    "BINARY_DIV",
    "BINARY_GT",
    "BINARY_LT",
    "UNARY_SUB", 
    "UNARY_NEG", 
    "ARG_PUSH", 
    "CALL_VOID", 
    "CALL", 
    "COPY",
    "CAST_REAL_TO_INT",
    "IF_FALSE",
    "GOTO"
};

static function_code_t generate_function_code(symbol_t* function_symbol);
static void generate_node_code(tac_t** list, node_t* node);
// returns addr where return value is stored
static size_t generate_valued_code(tac_t** list, node_t* node);

static size_t TAC_NEXT_LABEL = 0;
static size_t tac_emit(tac_t**, instruction_t, size_t, size_t, size_t);

static size_t new_temp();
static size_t new_int_const(long);
static size_t new_real_const(double);
static size_t new_string_idx_const(size_t);
static size_t new_label_ref(size_t);
static instruction_t instr_from_node_operator(operator_t);
static size_t get_symbol_addr(symbol_t* symbol);
static void generate_cast_expr(tac_t** list, type_info_t* info_src, size_t addr_src, type_info_t* info_dst, size_t addr_dst);

void generate_function_codes() {
    function_codes = da_init(function_code_t);
    addr_list = da_init(addr_t);
    // addr 0: UNUSED
    da_append(&addr_list, (addr_t){.type = ADDR_UNUSED});

    for (size_t i = 0; i < global_symbol_table->n_symbols; ++i) {
        symbol_t* function_symbol = global_symbol_table->symbols[i];
        if (function_symbol->type != SYMBOL_FUNCTION) continue;

        if (function_symbol->is_builtin) {
            // TODO: Should we have TAC for it?
            continue;
        }

        da_append(&function_codes, generate_function_code(function_symbol));
    }
}

static function_code_t generate_function_code(symbol_t* function_symbol) {

    function_code_t ret = (function_code_t){
        .function_symbol = function_symbol
    };
    ret.tac_list = da_init(tac_t);
    // child idx 3 is BLOCK
    generate_node_code(&ret.tac_list, function_symbol->node->children[3]);
    return ret;
}

static void generate_node_code(tac_t** list, node_t* node) {
    switch (node->type) {
        case BLOCK:
            {
                for (size_t i = 0; i < da_size(node->children); ++i) {
                    generate_node_code(list, node->children[i]);
                }
            }
            break;
        case RETURN_STATEMENT:
            {
                size_t ret_addr = 0;
                if (da_size(node->children) == 1) {
                    ret_addr = generate_valued_code(list, node->children[0]);
                }
                tac_emit(list, TAC_RETURN, ret_addr, 0, 0);
            }
            break;
        case FUNCTION_CALL:
            {
                symbol_t* function_symbol = node->children[0]->symbol;
                for (size_t i = 0; i < da_size(node->children[1]->children); ++i) {
                    size_t arg_addr = generate_valued_code(list, node->children[1]->children[i]);
                    tac_emit(list, TAC_ARG_PUSH, arg_addr, 0, 0);
                }
                size_t symbol_addr = get_symbol_addr(function_symbol);
                // If the function returns a value, it will not be saved.
                tac_emit(list, TAC_CALL_VOID, symbol_addr, 0, 0);
            }
            break;
        case VARIABLE_DECLARATION:
            {
                // Does nothing at this point
                if (da_size(node->children) <= 2) return;

                size_t assigned_addr = generate_valued_code(list, node->children[2]);
                assert(node->children[1]->type == IDENTIFIER);
                tac_emit(list, TAC_COPY, assigned_addr, 0, get_symbol_addr(node->children[1]->symbol));
            }
            break;
        case ASSIGNMENT_STATEMENT:
            {
                size_t dst_addr = get_symbol_addr(node->children[0]->symbol);
                size_t src_addr = generate_valued_code(list, node->children[1]);
                tac_emit(list, TAC_COPY, src_addr, 0, dst_addr);
            }
            break;
        case IF_STATEMENT:
            {
                size_t cond_addr = generate_valued_code(list, node->children[0]);
                size_t cmp_idx = tac_emit(list, TAC_IF_FALSE, cond_addr, 0, new_label_ref(0));
                // 'true' block
                generate_node_code(list, node->children[1]);

                if (da_size(node->children) == 3) {
                    size_t goto_idx = tac_emit(list, TAC_GOTO, 0, 0, new_label_ref(0));

                    // backpatch dst label of if statement
                    addr_list[(*list)[cmp_idx].dst].data.label = TAC_NEXT_LABEL;
                    generate_node_code(list, node->children[2]);

                    // backpatch dst label of jmp after if
                    addr_list[(*list)[goto_idx].dst].data.label = TAC_NEXT_LABEL;
                } else {
                    // backpatch dst label of if statement
                    addr_list[(*list)[cmp_idx].dst].data.label = TAC_NEXT_LABEL;
                }
                tac_emit(list, TAC_NOP, 0, 0, 0);
            }
            break;
        default:
            fprintf(stderr, "generate_node_code: Unhandled node type: %s\n", NODE_TYPE_NAMES[node->type]);
            exit(EXIT_FAILURE);
    }
}

static size_t generate_valued_code(tac_t** list, node_t* node) {
    switch (node->type) {
        case IDENTIFIER:
            {
                return get_symbol_addr(node->symbol);
            }
            break;
        case INTEGER_LITERAL:
            {
                return new_int_const(node->data.int_literal_value);
            }
            break;
        case REAL_LITERAL:
            {
                return new_real_const(node->data.real_literal_value);
            }
            break;
        case STRING_LITERAL:
            {
                return new_string_idx_const(node->data.string_literal_idx);
            }
            break;
        case CAST_EXPRESSION:
            {
                size_t to_cast_addr = generate_valued_code(list, node->children[1]);
                size_t result_addr = new_temp();
                generate_cast_expr(
                    list, 
                    node->children[1]->type_info, 
                    to_cast_addr, 
                    node->children[0]->type_info,
                    result_addr);
                return result_addr;
            }
            break;
        case FUNCTION_CALL:
            {
                symbol_t* function_symbol = node->children[0]->symbol;
                for (size_t i = 0; i < da_size(node->children[1]->children); ++i) {
                    size_t arg_addr = generate_valued_code(list, node->children[1]->children[i]);
                    tac_emit(list, TAC_ARG_PUSH, arg_addr, 0, 0);
                }
                size_t symbol_addr = get_symbol_addr(function_symbol);
                if (node->type_info->type_class == TC_BASIC && node->type_info->info.info_basic == TYPE_VOID) {
                    // Don't really know if this actually can happen
                    fail_node(node, "Expected %s to return a value, but return type is void\n", function_symbol->name);
                    exit(EXIT_FAILURE);
                } else {
                    size_t ret_addr = new_temp();
                    tac_emit(list, TAC_CALL, symbol_addr, 0, ret_addr);
                    return ret_addr;
                }
            }
            break;
        case OPERATOR:
            {
                if (da_size(node->children) == 1) {
                    // Unary
                    size_t src1_addr = generate_valued_code(list, node->children[0]);
                    size_t dst_addr = new_temp();
                    tac_emit(list, instr_from_node_operator(node->data.operator), src1_addr, 0, dst_addr);
                    return dst_addr;
                } else {
                    // Binary
                    size_t src1_addr = generate_valued_code(list, node->children[0]);
                    size_t src2_addr = generate_valued_code(list, node->children[1]);
                    size_t dst_addr = new_temp();
                    tac_emit(list, instr_from_node_operator(node->data.operator), src1_addr, src2_addr, dst_addr);
                    return dst_addr;
                }
            }
            break;
        case PARENTHESIZED_EXPRESSION:
            {
                return generate_valued_code(list, node->children[0]);
            }
            break;
        default:
            fprintf(stderr, "generate_valued_code: Unhandled node type: %s\n", NODE_TYPE_NAMES[node->type]);
            exit(EXIT_FAILURE);
    }
}

static size_t tac_emit(tac_t** list, instruction_t instr, size_t src1, size_t src2, size_t dst) {
    size_t insert_idx = da_size(*list);
    tac_t tac = (tac_t){
        .label = TAC_NEXT_LABEL++,
        .instr = instr,
        .src1  = src1,
        .src2  = src2,
        .dst   = dst,
    };
    da_append(list, tac);
    return insert_idx;
}

static size_t new_temp() {
    static size_t tmp_count = 0;
    size_t idx = da_size(addr_list);
    addr_t tmp_addr = (addr_t){
        .type = ADDR_TEMP,
        .data.temp_id = tmp_count++
    };
    da_append(&addr_list, tmp_addr);
    return idx;
}

static size_t new_int_const(long value) {
    size_t idx = da_size(addr_list);
    addr_t addr = (addr_t){
        .type = ADDR_INT_CONST,
        .data.int_const = value
    };
    da_append(&addr_list, addr);
    return idx;
}

static size_t new_real_const(double value) {
    size_t idx = da_size(addr_list);
    addr_t addr = (addr_t){
        .type = ADDR_REAL_CONST,
        .data.real_const = value
    };
    da_append(&addr_list, addr);
    return idx;
}

static size_t new_string_idx_const(size_t string_idx) {
    size_t idx = da_size(addr_list);
    addr_t addr = (addr_t) {
        .type = ADDR_STRING_CONST,
        .data.string_idx_const = string_idx
    };
    da_append(&addr_list, addr);
    return idx;
}

static size_t new_label_ref(size_t label) {
    size_t idx = da_size(addr_list);
    addr_t addr = (addr_t) {
        .type = ADDR_LABEL,
        .data.label = label
    };
    da_append(&addr_list, addr);
    return idx;
}

static instruction_t instr_from_node_operator(operator_t op) {
    switch (op) {
        case BINARY_ADD: return TAC_BINARY_ADD;
        case BINARY_SUB: return TAC_BINARY_SUB;
        case BINARY_MUL: return TAC_BINARY_MUL;
        case BINARY_DIV: return TAC_BINARY_DIV;
        case BINARY_GT: return TAC_BINARY_GT;
        case BINARY_LT: return TAC_BINARY_LT;
        case UNARY_SUB: return TAC_UNARY_SUB;
        case UNARY_NEG: return TAC_UNARY_NEG;
        default:
            {
                fprintf(stderr, "Instr from node operator unexpected operator %s\n", OPERATOR_TYPE_NAMES[op]);
                exit(EXIT_FAILURE);
            }
    }
}

static size_t get_symbol_addr(symbol_t* symbol) {
    assert(symbol != NULL);
    // TODO: its quadratic...
    for (size_t i = 0; i < da_size(addr_list); ++i) {
        addr_t addr = addr_list[i];
        if (addr.type != ADDR_SYMBOL) continue;
        if (addr.data.symbol != symbol) continue;
        return i;
    }
    size_t idx = da_size(addr_list);
    addr_t addr = (addr_t){
        .type = ADDR_SYMBOL,
        .data.symbol = symbol
    };
    da_append(&addr_list, addr);
    return idx;
}

static void generate_cast_expr(tac_t** list, type_info_t* info_src, size_t addr_src, type_info_t* info_dst, size_t addr_dst) {
    if (info_src->type_class == TC_BASIC && info_dst->type_class == TC_BASIC) {
        if (info_src->info.info_basic == TYPE_REAL && info_dst->info.info_basic == TYPE_INT) {
            tac_emit(list, TAC_CAST_REAL_INT, addr_src, 0, addr_dst);
        } else {
            fprintf(stderr, "Unhandled basic type conversion: \n");
            type_print(stderr, info_src);
            fprintf(stderr, "\n -> \n");
            type_print(stderr, info_dst);
            fprintf(stderr, "\n");
            assert(false && "Unhandled basic type combo");
        }
        return;
    } 
    assert(false && "Unhandled cast expr gen");
}

static void print_tac_addr(size_t addr_idx) {
    if (addr_idx == 0) return;
    addr_t addr = addr_list[addr_idx];
    switch(addr.type) {
        case ADDR_UNUSED:
            assert(false);
        case ADDR_SYMBOL:
            {
                printf(", (%s)", addr.data.symbol->name);
            }
            break;
        case ADDR_INT_CONST:
            {
                printf(", #%ld", addr.data.int_const);
            }
            break;
        case ADDR_REAL_CONST:
            {
                printf(", #%lf", addr.data.real_const);
            }
            break;
        case ADDR_STRING_CONST:
            {
                printf(", #s%zu [%s]", addr.data.string_idx_const, global_string_list[addr.data.string_idx_const]);
            }
            break;
        case ADDR_LABEL:
            {
                printf(", $%zu", addr.data.label);
            }
            break;
        case ADDR_TEMP:
            {
                printf(", t%ld", addr.data.temp_id);
            }
            break;
      }
}

static void print_tac_impl(tac_t tac) {
    printf("%s", TAC_INSTRUCTION_NAMES[tac.instr]);
    print_tac_addr(tac.src1);
    print_tac_addr(tac.src2);
    print_tac_addr(tac.dst);
    puts("");
}

void print_tac() {
    for (size_t func_idx = 0; func_idx < da_size(function_codes); ++func_idx) {
        printf("function %s\n", function_codes[func_idx].function_symbol->name);

        for (size_t i = 0; i < da_size(function_codes[func_idx].tac_list); ++i) {
            printf("%3zu:  ", function_codes[func_idx].tac_list[i].label);
            print_tac_impl(function_codes[func_idx].tac_list[i]);
        }
    }
}

