#ifndef GEN_H
#define GEN_H

#include <stdio.h>
#include "langc.h"

extern FILE * gen_outfile;

// Macros from TDT4205
#define RAX "%rax"
#define EAX "%eax" // lowest 32 bits of %rax
#define AL "%al"   // lowest byte of %rax
#define RBX "%rbx" // callee saved
#define RCX "%rcx"
#define RDX "%rdx"
#define RSP "%rsp" // callee saved
#define RBP "%rbp" // callee saved
#define RSI "%rsi"
#define RDI "%rdi"
#define R8 "%r8"
#define R9 "%r9"
#define R10 "%r10"
#define R11 "%r11"
#define R12 "%r12" // callee saved
#define R13 "%r13" // callee saved
#define R14 "%r14" // callee saved
#define R15 "%r15" // callee saved
#define RIP "%rip"
#define XMM0 "%xmm0"
#define XMM1 "%xmm1"

#define MEM(reg) "(" reg ")"
#define ARRAY_MEM(array, index, stride) "(" array "," index "," stride ")"

#define DIRECTIVE(fmt, ...) fprintf(gen_outfile, fmt "\n" __VA_OPT__(, ) __VA_ARGS__)
#define LABEL(name, ...) fprintf(gen_outfile, name ":\n" __VA_OPT__(, ) __VA_ARGS__)
#define EMIT(fmt, ...) fprintf(gen_outfile, "\t" fmt "\n" __VA_OPT__(, ) __VA_ARGS__)

#define MOVQ(src, dst) EMIT("movq %s, %s", (src), (dst))
#define MOVSD(src, dst) EMIT("movsd %s, %s", (src), (dst))
#define PUSHQ(src) EMIT("pushq %s", (src))
#define POPQ(src) EMIT("popq %s", (src))

#define ADDQ(src, dst) EMIT("addq %s, %s", (src), (dst))
#define SUBQ(src, dst) EMIT("subq %s, %s", (src), (dst))
#define NEGQ(reg) EMIT("negq %s", (reg))

#define IMULQ(src, dst) EMIT("imulq %s, %s", (src), (dst))
#define CQO EMIT("cqo");                 // Sign extend RAX -> RDX:RAX
#define IDIVQ(by) EMIT("idivq %s", (by)) // Divide RDX:RAX by "by", store result in RAX

#define RET EMIT("ret")

#define CMPQ(op1, op2) EMIT("cmpq %s, %s", (op1), (op2)) // Compare the two operands

// The SETcc-family of instructions assigns either 0 or 1 to a byte register, based on a comparison.
// The instruction immediately before the SETcc should be a
//   cmpq op1, op2
// The suffix given to SET, the "cc" part of "setcc", is the "condition code".
// It determines the kind of comparison being done.
// If the comparison is true, 1 is stored into "byte_reg". Otherwise 0 is stored.
#define SETE(byte_reg) EMIT("sete %s", (byte_reg))   // Store result of op1 == op2
#define SETNE(byte_reg) EMIT("setne %s", (byte_reg)) // Store result of op1 != op2
// NOTE: for inequality checks, the order of CMPQ's operands is the opposite of what you expect
// The following inequalities are all for signed integer operands
#define SETG(byte_reg) EMIT("setg %s", (byte_reg))   // Store result of op2 > op1
#define SETGE(byte_reg) EMIT("setge %s", (byte_reg)) // Store result of op2 >= op1
#define SETL(byte_reg) EMIT("setl %s", (byte_reg))   // Store result of op2 < op1
#define SETLE(byte_reg) EMIT("setle %s", (byte_reg)) // Store result of op2 <= op1

// Since set*-instructions assign to a byte register, we must extend the byte to fill
// an entire 64-bit register, using movzbq (move Zero-extend Byte to Quadword).
#define MOVZBQ(byte_reg, full_reg) \
  EMIT("movzbq %s, %s", (byte_reg), (full_reg)) // full_reg <- byte_reg

#define JNE(label) EMIT("jne %s", (label)) // Conditional jump (not equal)
#define JMP(label) EMIT("jmp %s", (label)) // Unconditional jump

// Bitwise and
#define ANDQ(src, dst) EMIT("andq %s, %s", (src), (dst))

// Do something else on Apple
#define ASM_BSS_SECTION ".bss"
#define ASM_STRING_SECTION ".rodata"
#define ASM_DECLARE_SYMBOLS ".global main"

void generate_program(void);

#endif // GEN_H
