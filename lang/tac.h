#ifndef TAC_H
#define TAC_H

#include "langc.h"

enum instruction_t {
    TAC_NOP,
    TAC_RETURN, // src1: addr to return
    TAC_BINARY_ADD,
    TAC_BINARY_SUB,
    TAC_BINARY_MUL,
    TAC_BINARY_DIV,
    TAC_BINARY_GT,
    TAC_BINARY_LT,
    TAC_UNARY_SUB, // -
    TAC_UNARY_NEG, // !
    TAC_ARG_PUSH, // push argument src1 before calling function
    TAC_CALL_VOID, // call function without return value, using args pushed from arg_push. 
    TAC_CALL, // same as above, but stores result in dst.
    TAC_COPY, // src1 -> dst
    TAC_CAST_REAL_INT,
    TAC_IF_FALSE, // ifFalse(src1) goto dst (label)
    TAC_GOTO, // unconditional jmp to dst
};

enum addr_type_t {
    ADDR_UNUSED,
    ADDR_SYMBOL,
    ADDR_INT_CONST,
    ADDR_REAL_CONST,
    ADDR_STRING_CONST,
    ADDR_LABEL,
    ADDR_TEMP
};

struct addr_t {
    addr_type_t type;
    union {
        symbol_t* symbol;
        long int_const;
        double real_const;
        size_t string_idx_const;
        long temp_id;
        size_t label;
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

void print_tac();

#endif // TAC_H
