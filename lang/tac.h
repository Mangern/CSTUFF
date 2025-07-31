#ifndef TAC_H
#define TAC_H

#include "langc.h"
#include "type.h"

#include <stdbool.h>

enum instruction_t {
    TAC_NOP,
    TAC_RETURN, // src1: addr to return
    TAC_BINARY_ADD,
    TAC_BINARY_SUB,
    TAC_BINARY_MUL,
    TAC_BINARY_DIV,
    TAC_BINARY_MOD,
    TAC_BINARY_GT,
    TAC_BINARY_LT,
    TAC_BINARY_GEQ,
    TAC_BINARY_LEQ,
    TAC_BINARY_EQ,
    TAC_BINARY_NEQ,
    TAC_UNARY_SUB, // -
    TAC_UNARY_NEG, // !
    TAC_CALL_VOID, // call function src1 without return value. src2 stores arg list
    TAC_CALL, // same as above, but stores result in dst.
    TAC_COPY, // src1 -> dst
    TAC_CAST_REAL_INT,
    TAC_IF_FALSE, // ifFalse(src1) goto dst (label)
    TAC_GOTO, // unconditional jmp to dst
    TAC_LOCOF, // store location of src1 into dst
    TAC_LOAD, // load src1[src2] -> dst. src1: location
    TAC_STORE, // store src1 -> src2[dst]. src2: location
    TAC_DECLARE_PARAM // declare src1 as an addr param
};

enum addr_type_t {
    ADDR_UNUSED,
    ADDR_SYMBOL,
    ADDR_INT_CONST,
    ADDR_REAL_CONST,
    ADDR_STRING_CONST,
    ADDR_BOOL_CONST,
    ADDR_LABEL,
    ADDR_TEMP,
    ADDR_ARG_LIST
};

struct addr_t {
    addr_type_t type;
    basic_type_t type_info;
    union {
        symbol_t* symbol;
        long int_const;
        double real_const;
        size_t string_idx_const;
        bool bool_const;
        long temp_id;
        size_t label;
        size_t* arg_addr_list; // TODO: unsure if want to store types or values
    } data;
};

struct tac_t {
    size_t label;
    instruction_t instr;
    // these are indexes in addr_list
    size_t src1;
    size_t src2;
    size_t dst;
};

struct function_code_t {
    symbol_t* function_symbol;
    tac_t* tac_list;
};

// list of all possible addresses
extern addr_t* addr_list;
extern function_code_t* function_codes;

void generate_function_codes();

void print_tac_addr(size_t addr_idx);
void print_tac();

#endif // TAC_H
