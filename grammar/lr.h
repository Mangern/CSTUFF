#ifndef LR_H
#define LR_H

#include "lex.h"
#include "../da/da.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    E_PRIME,
    E,
    T,
    F,
    Z,
    EPS,
    NUM_NONTERMINAL // technically terminal or idk
} NonTerminal;

#define SYMBOL_START E_PRIME
#define NUM_SYMBOLS (NUM_NONTERMINAL + NUM_TOKENS)

#define SYMBOL_NONTERM(v) (symbol_t) {.is_terminal=0, .symbol_val=v}

#define SYMBOL_TERM(v) (symbol_t) {.is_terminal=1, .symbol_val=v}

typedef struct {
    int symbol_val;
    bool is_terminal;
} symbol_t;

typedef struct {
    symbol_t start_symbol;
    da_t result_symbols;
} rule_t;

typedef struct {
    size_t rule_idx;
    size_t dot_pos;
} grammar_item_t;

typedef struct {
    da_t* items;
} grammar_itemset_t;

typedef enum {
    UNDEFINED,
    SHIFT,
    REDUCE,
    ACCEPT
} ActionType;

typedef struct {
    ActionType type;
    size_t argument;
} action_t;

typedef struct {
    da_t rules;
    da_t itemsets;
    int** table_goto;
    size_t ngoto;
    action_t** table_action;
} grammar_t;

void grammar_init(grammar_t* grammar);

void grammar_itemset_init(grammar_itemset_t* itemset);

bool grammar_itemsets_equal(grammar_itemset_t* itemset_a, grammar_itemset_t* itemset_b);

void grammar_itemset_copy(grammar_itemset_t* itemset_dest, grammar_itemset_t* itemset_src);

void grammar_add_rule(grammar_t* grammar, symbol_t start_symbol, size_t n_results, ...);

rule_t grammar_rule_at(grammar_t* grammar, size_t index);

void grammar_construct_itemsets(grammar_t* grammar);

void grammar_construct_slr(grammar_t* grammar);

size_t grammar_symbol_index(symbol_t symbol);

symbol_t grammar_symbol_from_index(size_t index);

void grammar_deinit(grammar_t* grammar);

void grammar_itemset_deinit(grammar_itemset_t* itemset);

char* grammar_symbol_str(symbol_t symbol);

void grammar_parse(grammar_t* grammar, char* str, size_t length);

void grammar_debug_print(FILE* out, grammar_t* grammar);

#endif // LR_H
