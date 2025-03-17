#ifndef NFA_H
#define NFA_H

#include "da.h"

typedef struct nfa_node_t nfa_node_t;
typedef struct nfa_t nfa_t;
typedef struct transition_t transition_t;

#define NFA_TRANS_EPS '\xff'

struct transition_t {
    char c;
    nfa_node_t* next_node;
};

struct nfa_node_t {
    transition_t* transitions;
};

struct nfa_t {
    nfa_node_t* initial_state;
    nfa_node_t* accepting_state;
    nfa_node_t** nodes;
};

void nfa_init(nfa_t* nfa);
void nfa_deinit(nfa_t* nfa);
void nfa_free_nodes(nfa_t* nfa);
void nfa_node_init(nfa_node_t* node);
void nfa_node_add_transition(nfa_node_t* node, char c, nfa_node_t* next);
void nfa_node_free(nfa_node_t* node);
int nfa_accepts(nfa_t* nfa, char* string, size_t size);
void nfa_node_transition(nfa_node_t* node, char c, nfa_node_t*** result);

void nfa_from_regex(char* regex_string, size_t size, nfa_t* nfa);

#endif // NFA_H
