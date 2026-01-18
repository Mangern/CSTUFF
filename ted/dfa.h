#ifndef DFA_H
#define DFA_H

#include <stdbool.h>

struct dfa_node_t {
    int id;
    int trans_cap;
    int trans_size;
    int *trans_char;
    bool final;
    struct dfa_node_t **trans_node;
};

typedef struct dfa_t {
    struct dfa_node_t *root;
    struct dfa_node_t *state;
} dfa_t;

void dfa_init(dfa_t *dfa);
void dfa_enter(dfa_t *dfa, int c);
struct dfa_node_t * dfa_add_transition(struct dfa_node_t *node, int c, int id);
void dfa_reset(dfa_t *dfa);

#endif // DFA_H
