#include "dfa.h"
#include "da.h"
#include "nfa.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct node_subset_t node_subset_t;
struct node_subset_t {
    da_t* nodes;
    node_subset_t* trans[256];
};

int node_ptr_cmp(const void* node_ptr_a, const void* node_ptr_b) {
    nfa_node_t* a = *(nfa_node_t**)node_ptr_a;
    nfa_node_t* b = *(nfa_node_t**)node_ptr_b;
    if (a < b) return -1;
    if (a == b) return 0;
    return 1;
}


node_subset_t* make_subset() {
    node_subset_t* subset = malloc(sizeof(node_subset_t));
    subset->nodes = malloc(sizeof(da_t));
    da_init(subset->nodes, sizeof(nfa_node_t*));
    return subset;
}

void subset_deinit(node_subset_t* subset) {
    da_deinit(subset->nodes);
    free(subset->nodes);
}

int node_subset_contains(node_subset_t* subset, nfa_node_t* node) {
    // Could binary search but...
    for (int i = 0; i < subset->nodes->size; ++i) {
        nfa_node_t* cur = *(nfa_node_t**)da_at(subset->nodes, i);
        if (cur == node) return 1;
    }
    return 0;
}

void node_subset_add(node_subset_t* subset, nfa_node_t* node) {
    da_push_back(subset->nodes, &node);
    // Stay sorted
    qsort(subset->nodes->buffer, subset->nodes->size, sizeof(nfa_node_t*), node_ptr_cmp);
}

int node_subset_equals(node_subset_t* subset_a, node_subset_t* subset_b) {
    if (subset_a->nodes->size != subset_b->nodes->size) return 0;

    // Assume sorted
    for (int i = 0; i < subset_a->nodes->size; ++i) {
        nfa_node_t* node_a = *(nfa_node_t**)da_at(subset_a->nodes, i);
        nfa_node_t* node_b = *(nfa_node_t**)da_at(subset_b->nodes, i);
        if (node_a != node_b) return 0;
    }
    return 1;
}

void epsilon_closure(nfa_node_t* node, node_subset_t* subset) {
    if (node_subset_contains(subset, node)) return;
    node_subset_add(subset, node);

    for (int i = 0; i < node->transitions->size; ++i) {
        transition_t trans = *(transition_t*)da_at(node->transitions, i);
        if (trans.c != NFA_TRANS_EPS) continue;
        epsilon_closure(trans.next_node, subset);
    }
}

void subset_epsilon_closure(node_subset_t* subset) {
    for (int i = 0; i < subset->nodes->size; ++i) {
        nfa_node_t* node = *(nfa_node_t**)da_at(subset->nodes, i);
        epsilon_closure(node, subset);
    }
}

dfa_t* dfa_from_nfa(nfa_t* nfa) {
    // Subset construction algorithm
    //  - The subset of nodes in the same epsilon closure (reachable by only traversing epsilon transitions)
    //    form a separate node in the dfa
    //      - Epsilon closure needs to be computed on this subset as well
    //  - The new subset of nodes reachable by a character from one subset defines the transition in the dfa
    //  - All nodes must have an exhaustive set of transitions. Error state on most of them (empty subset)
    //      - The algorithm terminates when all reachable subsets have been checked
    //      - Worst case should be 2^n possible subsets, but typically terminates way faster
    //  - If a subset contains the accepting state of the nfa, that node is accepting in the resulting dfa.

    node_subset_t* node_empty = make_subset();
    for (int i = 0; i < 256; ++i) {
        // It self-loops on every transition
        node_empty->trans[i] = node_empty;
    }

    da_t subset_nodes;
    da_init(&subset_nodes, sizeof(node_subset_t*));
    da_push_back(&subset_nodes, &node_empty);

    node_subset_t* node_init = make_subset();
    // Default is to go to empty
    for (int i = 0; i < 256; ++i) {
        node_init->trans[i] = node_empty;
    }
    da_push_back(&subset_nodes, &node_init);

    epsilon_closure(nfa->initial_state, node_init);

    da_t next_set;
    da_init(&next_set, sizeof(nfa_node_t*));

    for (int i = 1; i < subset_nodes.size; ++i) {
        node_subset_t* cur_subset = *(node_subset_t**)da_at(&subset_nodes, i);
        for (int c = 0; c < 256; ++c) {
            // Special case: c == EPS would cause wrongful transitions
            if ((char)c == (char)NFA_TRANS_EPS) {
                cur_subset->trans[c] = node_empty;
                continue;
            }
            // Transition all nodes in current subset on character c
            da_resize(&next_set, 0);
            for (int j = 0; j < cur_subset->nodes->size; ++j) {
                nfa_node_t* cur_node = *(nfa_node_t**)da_at(cur_subset->nodes, j);
                nfa_node_transition(cur_node, c, &next_set);
            }

            // Get the epsilon closure of the new set
            node_subset_t* next_subset_cand = make_subset();
            for (int j = 0; j < next_set.size; ++j) {
                nfa_node_t* node = *(nfa_node_t**)da_at(&next_set, j);
                epsilon_closure(node, next_subset_cand);
            }

            node_subset_t* next_subset = NULL;
            // Check if the resulting subset already exists
            for (int j = 0; j < subset_nodes.size; ++j) {
                node_subset_t* existing = *(node_subset_t**)da_at(&subset_nodes, j);
                if (node_subset_equals(next_subset_cand, existing)) {
                    next_subset = existing;
                    break;
                }
            }
            if (next_subset == NULL) {
                da_push_back(&subset_nodes, &next_subset_cand);
                next_subset = next_subset_cand;
            } else {
                subset_deinit(next_subset_cand);
                free(next_subset_cand);
            }

            cur_subset->trans[c] = next_subset;
        }
    }
    da_deinit(&next_set);

    dfa_t* dfa = malloc(sizeof(dfa_t));
    dfa->num_nodes = subset_nodes.size;
    assert(dfa->num_nodes >= 2);
    dfa->trans = malloc(dfa->num_nodes * sizeof(int*));
    dfa->node_flag = malloc(dfa->num_nodes * sizeof(int));
    memset(dfa->node_flag, 0, dfa->num_nodes * sizeof(int));

    dfa->node_flag[0] = DFA_FLAG_ERROR;
    dfa->node_flag[1] = DFA_FLAG_INITIAL;

    for (int i = 0; i < dfa->num_nodes; ++i) {
        dfa->trans[i] = malloc(256 * sizeof(int));

        node_subset_t* cur_subset = *(node_subset_t**)da_at(&subset_nodes, i);
        if (node_subset_contains(cur_subset, nfa->accepting_state)) {
            assert(i > 0);
            dfa->node_flag[i] |= DFA_FLAG_ACCEPT;
        }

        for (int c = 0; c < 256; ++c) {
            node_subset_t* next = cur_subset->trans[c];
            size_t index = da_indexof(&subset_nodes, &next);
            assert(index != -1);
            dfa->trans[i][c] = index;
        }

        subset_deinit(cur_subset);
        free(cur_subset);
    }

    da_deinit(&subset_nodes);

    return dfa;
}

void dfa_swap_nodes(dfa_t* dfa, int node_a, int node_b) {
    if (node_a == node_b) return;
    dfa->node_flag[node_a] ^= dfa->node_flag[node_b];
    dfa->node_flag[node_b] ^= dfa->node_flag[node_a];
    dfa->node_flag[node_a] ^= dfa->node_flag[node_b];

    for (int i = 0; i < 256; ++i) {
        dfa->trans[node_a][i] ^= dfa->trans[node_b][i];
        dfa->trans[node_b][i] ^= dfa->trans[node_a][i];
        dfa->trans[node_a][i] ^= dfa->trans[node_b][i];
    }

    for (int i = 0; i < dfa->num_nodes; ++i) {
        for (int c = 0; c < 256; ++c) {
            if (dfa->trans[i][c] == node_a)dfa->trans[i][c] = node_b;
            if (dfa->trans[i][c] == node_b)dfa->trans[i][c] = node_a;
        }
    }

}

void dfa_minimize(dfa_t* dfa) {
    // Myhill-Nerode "Table-filling" algorithm
    // Mark pairs of nodes as equivalent: two nodes are equivalent iff they are not distinguishable
    // A pair of nodes (q_a, q_b) are distinguishable if:
    // - One is final (accepting) and the other one is not (base case)
    // - (trans[q_a][c], trans[q_b][c]) are distinguishable for some c in the alphabet
    //
    // Not a very effective implementation, but a simple one

    int distinguishable[dfa->num_nodes][dfa->num_nodes];

    for (int i = 0; i < dfa->num_nodes; ++i) {
        memset(distinguishable[i], 0, dfa->num_nodes * sizeof(int));
    }

    // Initial base case
    for (int i = 0; i < dfa->num_nodes; ++i) {
        for (int j = i + 1; j < dfa->num_nodes; ++j) {
            if ((dfa->node_flag[i] & DFA_FLAG_ACCEPT) ^ (dfa->node_flag[j] & DFA_FLAG_ACCEPT)) {
                distinguishable[i][j] = 1;
            }
        }
    }

    // Use previous calculations to propagate distinguishability until 
    // no more pairs can be marked
    int marked = 1;
    while (marked) {
        marked = 0;

        for (int i = 0; i < dfa->num_nodes; ++i) {
            for (int j = i + 1; j < dfa->num_nodes; ++j) {
                if (distinguishable[i][j]) continue;

                for (int c = 0; c < 256; ++c) {
                    int t_i = dfa->trans[i][c];
                    int t_j = dfa->trans[j][c];
                    if (t_i == t_j) continue;
                    if (t_i > t_j) {
                        // swap
                        t_i ^= t_j;
                        t_j ^= t_i;
                        t_i ^= t_j;
                    }
                    if (distinguishable[t_i][t_j]) {
                        distinguishable[i][j] = 1;
                        marked = 1;
                        break;
                    }
                }
            }
        }
    }

    int remap[dfa->num_nodes];

    for (int i = 0; i < dfa->num_nodes; ++i)remap[i] = i;

    // If distinguishable[i][j] == 0, i and j are equivalent.
    // Remap the larger id onto the lowest id in its equivalence class
    // First calculate equivalence classes
    for (int i = 0; i < dfa->num_nodes; ++i) {
        for (int j = i + 1; j < dfa->num_nodes; ++j) {
            if (!distinguishable[i][j]) {
                remap[j] = remap[i];
            }
        }
    }

    // Perform actual remapping
    for (int i = 0; i < dfa->num_nodes; ++i) {
        for (int c = 0; c < 256; ++c) {
            dfa->trans[i][c] = remap[dfa->trans[i][c]];
        }
        dfa->node_flag[i] |= dfa->node_flag[remap[i]];
    }

    // Fill the holes in the dfa table by swapping downward
    int ptr = 0;
    for (int i = 0; i < dfa->num_nodes; ++i) {
        if (remap[i] != i) continue;
        dfa_swap_nodes(dfa, ptr, i);
        ++ptr;
    }

    // Finally, memory can be given up
    for (int i = ptr; i < dfa->num_nodes; ++i) {
        free(dfa->trans[i]);
    }

    // reallocarray is apparently smarter than realloc by handling overflow??
    dfa->trans     = reallocarray(dfa->trans, ptr, sizeof(int*));
    dfa->node_flag = reallocarray(dfa->node_flag, ptr, sizeof(int));

    dfa->num_nodes = ptr;
}

dfa_t* dfa_from_regex(char* regex_string, size_t size) {
    nfa_t* nfa = nfa_from_regex(regex_string, size);
    dfa_t* dfa = dfa_from_nfa(nfa);
    dfa_minimize(dfa);
    nfa_free_nodes(nfa);
    nfa_deinit(nfa);
    free(nfa);
    return dfa;
}

int dfa_accepts(dfa_t* dfa, char* string, size_t size) {
    int cur_state = 1; // Assumes initial state is 1

    for (int i = 0; i < size; ++i) {
        char c = string[i];
        cur_state = dfa->trans[cur_state][c];
        // Break early on error
        if (cur_state == 0) return 0;
    }
    return (dfa->node_flag[cur_state] & DFA_FLAG_ACCEPT) > 0;
}

void dfa_deinit(dfa_t *dfa) {
    for (int i = 0; i < dfa->num_nodes; ++i) {
        free(dfa->trans[i]);
    }
    free(dfa->trans);
    free(dfa->node_flag);
}
