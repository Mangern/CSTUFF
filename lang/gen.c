#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "da.h"
#include "fail.h"
#include "langc.h"
#include "symbol.h"
#include "symbol_table.h"
#include "tree.h"
#include "type.h"
#include "tac.h"

FILE *gen_outfile = 0;

#define NUM_REGISTER_PARAMS 6
static const char* REGISTER_PARAMS[6] = {RDI, RSI, RDX, RCX, R8, R9};

// node** : 
#define FUNCTION_ARGS(func) ((func)->node->children[1]->children[0]->children)

static void generate_stringtable();
static void generate_global_variables();
static void generate_constants();
static void generate_function(function_code_t);
static void generate_tac(tac_t);
static void preprocess_tac_list(tac_t* tac_list);
static void generate_safe_putchar();
static void generate_safe_printf();
static void generate_main_function();

static size_t* addr_frame_location;

void generate_program() {
    if (gen_outfile == 0) {
        gen_outfile = stdout;
    }

    generate_stringtable();
    generate_constants();

    generate_global_variables();

    DIRECTIVE(".text");

    addr_frame_location = malloc(sizeof(size_t) * da_size(addr_list));

    for (size_t i = 0; i < da_size(function_codes); ++i) {
        generate_function(function_codes[i]);
    }

    generate_main_function();
}

static void generate_stringtable() {
    DIRECTIVE(".section %s", ASM_STRING_SECTION);
    // These strings are used by printf
    DIRECTIVE("intout: .asciz \"%s\"", "%ld");
    DIRECTIVE("strout: .asciz \"%s\"", "%s");
    DIRECTIVE("realout: .asciz \"%s\"", "%f");
    DIRECTIVE("sizeout: .asciz \"%s\"", "%zu");
    // This string is used by the entry point-wrapper
    DIRECTIVE("errout: .asciz \"%s\"", "Wrong number of arguments");

    for (size_t i = 0; i < da_size(global_string_list); i++)
        DIRECTIVE("string%ld: \t.asciz \"%s\"", i, global_string_list[i]);

}

static void generate_global_variables() {
    DIRECTIVE(".section %s", ASM_BSS_SECTION);
    DIRECTIVE(".align 8");

    for (size_t i = 0; i < global_symbol_table->n_symbols; ++i) {
        symbol_t *sym = global_symbol_table->symbols[i];
        if (sym->type != SYMBOL_GLOBAL_VAR)
            continue;

        type_info_t* type = sym->node->type_info;
        assert(type != NULL);

        // TODO: Currently no initial values
        DIRECTIVE(".%s: .zero %zu", sym->name, type_sizeof(type));
    }
}

static void generate_constants() {
    for (size_t i = 0; i < da_size(addr_list); ++i) {
        addr_t addr = addr_list[i];

        switch (addr.type) {
            case ADDR_REAL_CONST:
                DIRECTIVE("CONST%zu: .quad %ld", i, *(long*)&addr.data.real_const);
                break;
            case ADDR_INT_CONST:
            case ADDR_BOOL_CONST:
            case ADDR_UNUSED:
            case ADDR_SYMBOL:
            case ADDR_STRING_CONST:
            case ADDR_LABEL:
            case ADDR_TEMP:
            case ADDR_ARG_LIST:
            case ADDR_SIZE_CONST:
                break;
        }
    }
}

static symbol_t* current_function;
static size_t* current_used_addrs = 0;
static int* is_jmp_dst = 0; // true if label is used as a jump destination

static void generate_function(function_code_t func_code) {
    current_function = func_code.function_symbol;

    LABEL(".%s", current_function->name);
    PUSHQ(RBP);
    MOVQ(RSP, RBP);

    // Push register args to stack

    size_t local_space = 0;
    size_t home_space = 0;

    for (size_t i = 0; i < da_size(FUNCTION_ARGS(current_function)) && i < NUM_REGISTER_PARAMS; ++i) {
        // lol
        EMIT("pushq %s // %s", REGISTER_PARAMS[i], FUNCTION_ARGS(current_function)[i]->children[0]->symbol->name);
        home_space += 8;
    }

    preprocess_tac_list(func_code.tac_list);

    for (size_t i = 0; i < da_size(current_used_addrs); ++i) {
        addr_t addr = addr_list[current_used_addrs[i]];
        switch (addr.type) {
        case ADDR_SYMBOL:
            {
                symbol_t* sym = addr.data.symbol;
                
                if (sym->type == SYMBOL_LOCAL_VAR || sym->type == SYMBOL_LOCAL_STRUCT) {
                    assert(sym->node != NULL);
                    assert(sym->node->type_info != NULL);
                    local_space += type_sizeof(sym->node->type_info);
                    addr_frame_location[current_used_addrs[i]] = local_space + home_space;
                }
            }
            break;
        case ADDR_TEMP:
            {
                // TODO: temporary with other size?
                local_space += 8;
                addr_frame_location[current_used_addrs[i]] = local_space + home_space;
            }
            break;
        case ADDR_ARG_LIST:
            {
            }
            break;
        case ADDR_LABEL:
        case ADDR_UNUSED:
        case ADDR_INT_CONST:
        case ADDR_SIZE_CONST:
        case ADDR_REAL_CONST:
        case ADDR_STRING_CONST:
        case ADDR_BOOL_CONST:
          break;
        }
    }

    size_t frame_space = local_space + home_space;

    if (frame_space & 0xF) {
        frame_space += 8;
    }

    EMIT("subq $%zu, %s", frame_space - home_space, RSP);


    for (size_t i = 0; i < da_size(func_code.tac_list); ++i) {
        tac_t tac = func_code.tac_list[i];
        if (tac.label < da_size(is_jmp_dst) && is_jmp_dst[tac.label]) {
            LABEL("L%zu", tac.label);
        }
        generate_tac(tac);
    }

    LABEL(".%s.epilogue", current_function->name);
    MOVQ(RBP, RSP);
    POPQ(RBP);
    RET;
}

static long get_addr_rbp_offset(size_t addr_idx) {
    addr_t addr = addr_list[addr_idx];
    if (addr.type == ADDR_SYMBOL && addr.data.symbol->type == SYMBOL_PARAMETER) {
        if (addr.data.symbol->sequence_number >= NUM_REGISTER_PARAMS) {
            long offs = (long)addr.data.symbol->sequence_number - NUM_REGISTER_PARAMS + 2;
            offs *= 8;
            return offs;
        }
        long offs = (long)addr.data.symbol->sequence_number;
        offs++;
        offs *= 8;
        return -offs;
    }

    return -(long)addr_frame_location[addr_idx];
}

static const char* generate_addr_access(size_t addr_idx) {
    static char result[420];
    addr_t addr = addr_list[addr_idx];
    memset(result, 0, sizeof result);
    switch (addr.type) {
        case ADDR_INT_CONST:
            {
                snprintf(result, sizeof result, "$%ld", addr.data.int_const);
            }
            break;
        case ADDR_SIZE_CONST:
            {
                snprintf(result, sizeof result, "$%zu", addr.data.size_const);
            }
            break;
        case ADDR_BOOL_CONST:
            {
                snprintf(result, sizeof result, "$%d", addr.data.bool_const);
            }
            break;
        case ADDR_REAL_CONST:
            {
                snprintf(result, sizeof result, "CONST%zu(%s)", addr_idx, RIP);
            }
            break;
        case ADDR_STRING_CONST:
            {
                snprintf(result, sizeof result, "string%zu(%s)", addr.data.string_idx_const, RIP);
            }
            break;
        case ADDR_SYMBOL:
            {
                symbol_t* sym = addr.data.symbol;

                if (sym->type == SYMBOL_LOCAL_VAR || sym->type == SYMBOL_PARAMETER || sym->type == SYMBOL_LOCAL_STRUCT) {
                    long rbp_offset = get_addr_rbp_offset(addr_idx);
                    snprintf(result, sizeof result, "%ld(%s)", rbp_offset, RBP);
                } else if (sym->type == SYMBOL_GLOBAL_VAR) {
                    snprintf(result, sizeof result, ".%s(%s)", sym->name, RIP);
                } else {
                    assert(false && "Not implemented");
                }
            }
            break;
        case ADDR_TEMP:
            {
                long rbp_offset = get_addr_rbp_offset(addr_idx);
                snprintf(result, sizeof result, "%ld(%s)", rbp_offset, RBP);
            }
            break;
        default:
            assert(false && "Not implemented");
            break;
    }
    return result;
}

static void generate_tac(tac_t tac) {
    switch (tac.instr) {
    case TAC_NOP:
    case TAC_DECLARE_PARAM:
        return;
    case TAC_RETURN:
        {
            if (tac.src1 != 0) {
                MOVQ(generate_addr_access(tac.src1), RAX);
            }
            EMIT("jmp .%s.epilogue", current_function->name);
        }
        break;
    case TAC_BINARY_ADD:
    case TAC_BINARY_SUB:
    case TAC_BINARY_MUL:
    case TAC_BINARY_DIV:
    case TAC_BINARY_MOD:
    case TAC_BINARY_GT:
    case TAC_BINARY_LT:
    case TAC_BINARY_GEQ:
    case TAC_BINARY_LEQ:
    case TAC_BINARY_EQ:
    case TAC_BINARY_NEQ:
        {
            char* SRC1_REG = RAX;
            char* SRC2_REG = RCX;
            char* TYPE_SUF = "q";
            bool is_float = 0;

            if (addr_list[tac.dst].type_info == TYPE_REAL) {
                SRC1_REG = XMM0;
                SRC2_REG = XMM1;
                TYPE_SUF = "sd";
                is_float = 1;
                MOVSD(generate_addr_access(tac.src1), SRC1_REG);
                MOVSD(generate_addr_access(tac.src2), SRC2_REG);
            } else {
                MOVQ(generate_addr_access(tac.src1), SRC1_REG);
                MOVQ(generate_addr_access(tac.src2), SRC2_REG);
            }


            switch (tac.instr) {
                case TAC_BINARY_ADD:
                    EMIT("add%s %s, %s", TYPE_SUF, SRC2_REG, SRC1_REG); // rax += rcx
                    break;
                case TAC_BINARY_SUB:
                    EMIT("sub%s %s, %s", TYPE_SUF, SRC2_REG, SRC1_REG); // rax -= rcx
                    break;
                case TAC_BINARY_MUL:
                    if (is_float) {
                        EMIT("mulsd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        IMULQ(SRC2_REG, SRC1_REG); // rax *= rcx
                    }
                    break;
                case TAC_BINARY_DIV:
                    if (is_float) {
                        EMIT("divsd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        CQO; // sign extend rax to rdx:rax
                        IDIVQ(SRC2_REG); // rdx:rax /= rcx
                    }
                    break;
                case TAC_BINARY_MOD:
                    if (is_float) {
                        fprintf(stderr, "Cannot mod floats\n");
                        exit(EXIT_FAILURE);
                    } else {
                        CQO;
                        IDIVQ(SRC2_REG); // rdx:rax /= rcx
                        MOVQ(RDX, SRC1_REG);
                    }
                    break;
                case TAC_BINARY_GT:
                    if (is_float) {
                        EMIT("comisd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        CMPQ(SRC2_REG, SRC1_REG);
                    }
                    SETG(AL);
                    MOVZBQ(AL, SRC1_REG);
                    break;
                case TAC_BINARY_LT:
                    if (is_float) {
                        EMIT("comisd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        CMPQ(SRC2_REG, SRC1_REG);
                    }
                    SETL(AL);
                    MOVZBQ(AL, SRC1_REG);
                    break;
                case TAC_BINARY_GEQ:
                    if (is_float) {
                        EMIT("comisd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        CMPQ(SRC2_REG, SRC1_REG);
                    }
                    SETGE(AL);
                    MOVZBQ(AL, SRC1_REG);
                    break;
                case TAC_BINARY_LEQ:
                    if (is_float) {
                        EMIT("comisd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        CMPQ(SRC2_REG, SRC1_REG);
                    }
                    SETLE(AL);
                    MOVZBQ(AL, SRC1_REG);
                    break;
                case TAC_BINARY_EQ:
                    if (is_float) {
                        EMIT("comisd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        CMPQ(SRC2_REG, SRC1_REG);
                    }
                    SETE(AL);
                    MOVZBQ(AL, SRC1_REG);
                    break;
                case TAC_BINARY_NEQ:
                    if (is_float) {
                        EMIT("comisd %s, %s", SRC2_REG, SRC1_REG);
                    } else {
                        CMPQ(SRC2_REG, SRC1_REG);
                    }
                    SETNE(AL);
                    MOVZBQ(AL, SRC1_REG);
                    break;
                default:
                    assert(false);
            }

            MOVQ(SRC1_REG, generate_addr_access(tac.dst));
        }
        break;
    case TAC_CALL_VOID:
        {
            addr_t called_addr = addr_list[tac.src1];
            symbol_t* called_func = called_addr.data.symbol;
            if (called_func->is_builtin) {
                if (strncmp(called_func->name, "print", 5) == 0) {
                    addr_t addr_arg_list = addr_list[tac.src2];

                    for (size_t i = 0; i < da_size(addr_arg_list.data.arg_addr_list); ++i) {
                        if (i > 0) {
                            MOVQ("$' '", RDI);
                            EMIT("call safe_putchar");
                        }


                        size_t arg_idx = addr_arg_list.data.arg_addr_list[i];
                        addr_t arg = addr_list[arg_idx];

                        // TODO: Only works with string constants (not variables)
                        if (arg.type_info == TYPE_STRING) {
                            EMIT("leaq strout(%s), %s", RIP, RDI);
                            EMIT("leaq %s, %s", generate_addr_access(arg_idx), RSI);
                        } else if (arg.type_info == TYPE_INT) {
                            MOVQ(generate_addr_access(arg_idx), RSI);
                            EMIT("leaq intout(%s), %s", RIP, RDI);
                        } else if (arg.type_info == TYPE_REAL) {
                            EMIT("leaq realout(%s), %s", RIP, RDI);
                            MOVSD(generate_addr_access(arg_idx), XMM0);
                        } else if (arg.type_info == TYPE_BOOL) {
                            MOVQ(generate_addr_access(arg_idx), RSI);
                            EMIT("leaq intout(%s), %s", RIP, RDI);
                        } else if (arg.type_info == TYPE_SIZE) {
                            MOVQ(generate_addr_access(arg_idx), RSI);
                            EMIT("leaq sizeout(%s), %s", RIP, RDI);
                        } else {
                            assert(false && "Not implemented");
                        }
                        MOVQ("$1", RAX);
                        EMIT("call safe_printf");
                    }
                        
                    if (strncmp(called_func->name, "println", 7) == 0) {
                        MOVQ("$'\\n'", RDI);
                        EMIT("call safe_putchar");
                    }
                } else {
                    assert(false && "Not implemented");
                }
            } else {
                symbol_t* function_symbol = addr_list[tac.src1].data.symbol;
                addr_t addr_arg_list = addr_list[tac.src2];

                long num_params = da_size(addr_arg_list.data.arg_addr_list);

                size_t stack_arg_space = 0;

                if (num_params > NUM_REGISTER_PARAMS) {
                    stack_arg_space = (num_params - NUM_REGISTER_PARAMS) * 8;
                }

                if (stack_arg_space & 0xF) {
                    PUSHQ("$0");
                    stack_arg_space += 8;
                }

                for (long i = num_params - 1; i >= 0; --i) {
                    size_t arg_idx = addr_arg_list.data.arg_addr_list[i];
                    addr_t arg = addr_list[arg_idx];
                    if (arg.type_info == TYPE_CHAR) {
                        EMIT("leaq %s, %s", generate_addr_access(arg_idx), RAX);
                        PUSHQ(RAX);
                    } else if (arg.type_info == TYPE_INT || arg.type_info == TYPE_SIZE) {
                        MOVQ(generate_addr_access(arg_idx), RAX);
                        PUSHQ(RAX);
                    } else if (arg.type_info == TYPE_REAL) {
                        MOVSD(generate_addr_access(arg_idx), XMM0);
                        PUSHQ(XMM0);
                    } else {
                        assert(false && "Not implemented");
                    }
                }

                for (long i = 0; i < num_params && i < NUM_REGISTER_PARAMS; ++i) {
                    POPQ(REGISTER_PARAMS[i]);
                }
                EMIT("call .%s", function_symbol->name);

                // restore stack
                if (num_params > NUM_REGISTER_PARAMS)
                {
                    EMIT("addq $%zu, %s", stack_arg_space, RSP);
                }
            }
        }
        break;
    case TAC_CALL:
        {
            //assert(false);
            symbol_t* function_symbol = addr_list[tac.src1].data.symbol;
            assert(!function_symbol->is_builtin && "No builtin function with return value");

            addr_t addr_arg_list = addr_list[tac.src2];
            long num_params = da_size(addr_arg_list.data.arg_addr_list);

            size_t stack_arg_space = 0;

            if (num_params > NUM_REGISTER_PARAMS) {
                stack_arg_space = (num_params - NUM_REGISTER_PARAMS) * 8;
            }

            if (stack_arg_space & 0xF) {
                PUSHQ("$0");
                stack_arg_space += 8;
            }

            for (long i = num_params - 1; i >= 0; --i) {
                size_t arg_idx = addr_arg_list.data.arg_addr_list[i];
                addr_t arg = addr_list[arg_idx];
                if (arg.type_info == TYPE_CHAR) {
                    EMIT("leaq %s, %s", generate_addr_access(arg_idx), RAX);
                    PUSHQ(RAX);
                } else if (arg.type_info == TYPE_INT) {
                    MOVQ(generate_addr_access(arg_idx), RAX);
                    PUSHQ(RAX);
                } else if (arg.type_info == TYPE_REAL) {
                    MOVSD(generate_addr_access(arg_idx), XMM0);
                    PUSHQ(XMM0);
                } else {
                    assert(false && "Not implemented");
                }
            }


            for (long i = 0; i < num_params && i < NUM_REGISTER_PARAMS; ++i) {
                POPQ(REGISTER_PARAMS[i]);
            }
            EMIT("call .%s", addr_list[tac.src1].data.symbol->name);

            // restore stack
            if (num_params > NUM_REGISTER_PARAMS)
            {
                EMIT("addq $%zu, %s", stack_arg_space, RSP);
            }
            // store result
            MOVQ(RAX, generate_addr_access(tac.dst));
        }
        break;
    case TAC_ALLOC:
        {
            MOVQ(generate_addr_access(tac.src1), RDI);
            EMIT("call safe_malloc");
            MOVQ(RAX, generate_addr_access(tac.dst));
        }
        break;
    case TAC_COPY:
        {
            MOVQ(generate_addr_access(tac.src1), RAX);
            MOVQ(RAX, generate_addr_access(tac.dst));
        }
        break;
    case TAC_IF_FALSE:
        {
            MOVQ(generate_addr_access(tac.src1), RAX);
            CMPQ("$0", RAX);
            EMIT("je L%zu", addr_list[tac.dst].data.label);
        }
        break;
    case TAC_GOTO:
        {
            EMIT("jmp L%zu", addr_list[tac.dst].data.label);
        }
        break;
    case TAC_CAST_REAL_INT:
        {
            MOVQ(generate_addr_access(tac.src1), "%xmm0");
            EMIT("cvttsd2si %s, %s", "%xmm0", RAX);
            MOVQ(RAX, generate_addr_access(tac.dst));
        }
        break;
    case TAC_LOCOF:
        {
            EMIT("leaq %s, %s", generate_addr_access(tac.src1), RAX);
            MOVQ(RAX, generate_addr_access(tac.dst));
        }
        break;
    case TAC_STORE:
        {
            // src1 -> src2[dst]
            // Load base of array
            MOVQ(generate_addr_access(tac.src2), RAX);

            // load index of array
            MOVQ(generate_addr_access(tac.dst), RCX);

            // store exact memory location in rax
            // TODO: preceding multiplication can be put in this
            //       instruction
            EMIT("leaq (%s, %s, 1), %s", RAX, RCX, RAX);

            // What we want to store
            MOVQ(generate_addr_access(tac.src1), RCX);

            MOVQ(RCX, MEM(RAX));
        }
        break;
    case TAC_LOAD:
        {
            // src1[src2] -> dst
            // Load base
            MOVQ(generate_addr_access(tac.src1), RAX);

            // Load index

            if (tac.src2 > 0) {
                MOVQ(generate_addr_access(tac.src2), RCX);
            } else {
                MOVQ("$0", RCX);
            }

            // store exact location
            EMIT("leaq (%s, %s, 1), %s", RAX, RCX, RAX);

            MOVQ(MEM(RAX), RCX);

            MOVQ(RCX, generate_addr_access(tac.dst));
        }
        break;
    case TAC_UNARY_NEG:
        {
            MOVQ(generate_addr_access(tac.src1), RCX);
            EMIT("xor %s, %s", RAX, RAX);
            EMIT("test %s, %s", RCX, RCX);
            SETE(AL);
            MOVQ(RAX, generate_addr_access(tac.dst));
        }
        break;
    case TAC_UNARY_SUB:
        {
            if (addr_list[tac.src1].type_info == TYPE_REAL) {
                assert(false && "Not implemented");
            }
            MOVQ(generate_addr_access(tac.src1), RAX);
            EMIT("neg %s", RAX);
            MOVQ(RAX, generate_addr_access(tac.dst));
        }
        break;
    default:
        assert(false && "not implemented");
    }

}

int comp_sizet(const void* a, const void* b) {
    return ((int)*(size_t*)a - (int)*(size_t*)b);
}

static void preprocess_tac_list(tac_t* tac_list) {
    // generate unique sorted list of addrs used in the tac_list
    da_clear(current_used_addrs);

    for (size_t i = 0; i < da_size(tac_list); ++i) {
        tac_t tac = tac_list[i];
        if (tac.src1 != 0) {
            da_append(current_used_addrs, tac.src1);
        }
        if (tac.src2 != 0) {
            da_append(current_used_addrs, tac.src2);
        }
        if (tac.dst != 0) {
            da_append(current_used_addrs, tac.dst);
        }

        if (addr_list[tac.dst].type == ADDR_LABEL) {
            // TODO: refactor
            size_t dst_label = addr_list[tac.dst].data.label;
            while (da_size(is_jmp_dst) <= dst_label) {
                da_append(is_jmp_dst, 0);
            }
            is_jmp_dst[dst_label] = 1;
        }
    }

    // Filter unique used addrs
    qsort(current_used_addrs, da_size(current_used_addrs), sizeof(size_t), comp_sizet);
    size_t ptr = 0;

    for (size_t i = 1; i < da_size(current_used_addrs); ++i) {
        if (i > ptr && current_used_addrs[ptr] == current_used_addrs[i]) continue;
        current_used_addrs[++ptr] = current_used_addrs[i];
    }
    da_resize(current_used_addrs, ptr+1);
}

static void generate_safe_putchar(void)
{
    LABEL("safe_putchar");

    PUSHQ(RBP);
    MOVQ(RSP, RBP);
    // This is a bitmask that abuses how negative numbers work, to clear the last 4 bits
    // A stack pointer that is not 16-byte aligned, will be moved down to a 16-byte boundary
    ANDQ("$-16", RSP);
    EMIT("call putchar");
    // Cleanup the stack back to how it was
    MOVQ(RBP, RSP);
    POPQ(RBP);
    RET;
}

static void generate_safe_printf(void)
{
    LABEL("safe_printf");

    PUSHQ(RBP);
    MOVQ(RSP, RBP);
    // This is a bitmask that abuses how negative numbers work, to clear the last 4 bits
    // A stack pointer that is not 16-byte aligned, will be moved down to a 16-byte boundary
    ANDQ("$-16", RSP);
    EMIT("call printf");
    // Cleanup the stack back to how it was
    MOVQ(RBP, RSP);
    POPQ(RBP);
    RET;
}

static void generate_safe_malloc(void)
{
    LABEL("safe_malloc");

    PUSHQ(RBP);
    MOVQ(RSP, RBP);
    ANDQ("$-16", RSP);
    EMIT("call malloc");
    MOVQ(RBP, RSP);
    POPQ(RBP);
    RET;
}

static void generate_main_function() {
    LABEL("main");

    PUSHQ(RBP);
    MOVQ(RSP, RBP);
    ANDQ("$-16", RSP);

    // TODO: argc, argv
    EMIT("call .main");
    // TODO: return value
    MOVQ(RBP, RSP);
    POPQ(RBP);

    MOVQ("$0", RDI);
    EMIT("call exit");

    generate_safe_printf();
    generate_safe_putchar();
    generate_safe_malloc();

    DIRECTIVE("%s", ASM_DECLARE_MAIN);
}
