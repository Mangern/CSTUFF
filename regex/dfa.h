#ifndef DFA_H
#define DFA_H

#include "nfa.h"
#include <stddef.h>

typedef struct dfa_t dfa_t;
typedef struct dfa_node_t dfa_node_t;

#define DFA_FLAG_INITIAL 1
#define DFA_FLAG_ACCEPT 2
#define DFA_FLAG_ERROR 4

struct dfa_t {
    int num_nodes;
    // num_nodes x 256 table
    int** trans;
    int* node_flag;
};

dfa_t* dfa_from_nfa(nfa_t* nfa);
dfa_t* dfa_from_regex(char* regex_string, size_t size);
void dfa_minimize(dfa_t* dfa);
int    dfa_accepts(dfa_t* dfa, char* string, size_t size);
// Match maximal prefix of string. Return the length of the match.
size_t dfa_match(dfa_t* dfa, char* string, size_t max_len);
void   dfa_deinit(dfa_t* dfa);

#endif // DFA_H
