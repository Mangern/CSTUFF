#include "dfa.h"
#include <stdlib.h>
#include <string.h>

typedef struct dfa_node_t dfa_node_t;

static dfa_node_t* get_transition(struct dfa_node_t *node, int c);

void dfa_init(dfa_t *dfa) {
    dfa->root = calloc(1, sizeof(dfa_node_t));
    dfa->state = dfa->root;
}

void dfa_enter(dfa_t *dfa, int c) {
    dfa_node_t *next = get_transition(dfa->state, c);
    if (next == 0) return;
    dfa->state = next;
}

dfa_node_t* dfa_add_transition(dfa_node_t *node, int c, int id) {
    if (node->trans_size == node->trans_cap) {
        int new_cap = node->trans_cap * 2;
        if (new_cap == 0) {
            new_cap = 256;
        }

        dfa_node_t ** new_trans = calloc(new_cap, sizeof(void*));
        int * new_trans_char = calloc(new_cap, sizeof(int));

        for (int i = 0; i < node->trans_size; ++i) {
            new_trans[i] = node->trans_node[i];
            new_trans_char[i] = node->trans_char[i];
        }

        if (node->trans_cap > 0) {
            free(node->trans_node);
            free(node->trans_char);
        }

        node->trans_node = new_trans;
        node->trans_char = new_trans_char;
    }

    int idx = node->trans_size++;

    dfa_node_t *new = calloc(1, sizeof(dfa_node_t));
    new->id = id;
    new->final = true;

    node->trans_node[idx] = new;
    node->trans_char[idx] = c;

    return new;
}

void dfa_reset(dfa_t *dfa) {
    dfa->state = dfa->root;
}

// ========== Internal ==========

static dfa_node_t* get_transition(struct dfa_node_t *node, int c) {
    for (int i = 0; i < node->trans_size; ++i) {
        if (node->trans_char[i] == c) {
            return node->trans_node[i];
        }
    }
    return 0;
}
