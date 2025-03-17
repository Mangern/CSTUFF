#include "nfa.h"
#include "da.h"
#include "preprocess.h"

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void nfa_init(nfa_t *nfa) {
    nfa->nodes = da_init(nfa_node_t*);
}

void nfa_deinit(nfa_t *nfa) {
    // NOTE: does not free up nodes. Only the local node pointer storage
    da_deinit(&nfa->nodes);
}

void nfa_free_nodes(nfa_t *nfa) {
    // Separate from deinit, because sometimes nodes need to live longer
    // mindfuck lifetime business
    for (int i = 0; i < da_size(nfa->nodes); ++i) {
        nfa_node_t* node = nfa->nodes[i];
        nfa_node_free(node);
    }
}

void nfa_node_init(nfa_node_t* node) {
    node->transitions = da_init(transition_t);
}

void nfa_node_free(nfa_node_t* node) {
    da_deinit(&node->transitions);
    free(node);
}


void nfa_node_add_transition(nfa_node_t* node, char c, nfa_node_t* next) {
    transition_t trans = {c, next};
    da_append(&node->transitions, trans);
}



int nfa_accepts_impl(nfa_t* nfa, nfa_node_t* cur_state, char* string, size_t size) {
    if (size == 0) {
        if (nfa->accepting_state == cur_state) return 1;
    }

    int ret = 0;
    for (int i = 0; i < da_size(cur_state->transitions); ++i) {
        transition_t trans = cur_state->transitions[i];
        if (size > 0 && trans.c == string[0]) {
            ret |= nfa_accepts_impl(nfa, trans.next_node, string+1, size - 1);
        }
        if (trans.c == NFA_TRANS_EPS) {
            // Hopefully no eps loop lollll
            ret |= nfa_accepts_impl(nfa, trans.next_node, string, size);
        }
    }

    return ret;
}

int nfa_accepts(nfa_t* nfa, char* string, size_t size) {
    return nfa_accepts_impl(nfa, nfa->initial_state, string, size);
}

void nfa_node_transition(nfa_node_t* node, char c, nfa_node_t*** result) {
    for (int i = 0; i < da_size(node->transitions); ++i) {

        transition_t trans = node->transitions[i];
        if (trans.c == c) {
            da_append(result, trans.next_node);
        }
    }
}

int* regex_paren_match(char* regex_string, size_t size) {
    int* paren_match = malloc(size * sizeof(int));
    memset(paren_match, -1, size * sizeof(int));
    int* paren_stack = da_init(int);

    for (int i = 0; i < size; ++i) {
        if (regex_string[i] == '(') {
            da_append(&paren_stack, i);
        } else if (regex_string[i] == ')') {
            assert(da_size(paren_stack) > 0); // Else, unmatched rparen

            int loc = paren_stack[da_size(paren_stack) - 1];
            paren_match[loc] = i;
            paren_match[i] = loc;
            da_pop(&paren_stack);
        }
    }


    assert(da_size(paren_stack) == 0); // unmatched lparen
    da_deinit(&paren_stack);
    return paren_match;
}

nfa_t* concat_nfas(nfa_t** concat_list) {
    nfa_t* ret = concat_list[0];
    for (int i = 1; i < da_size(concat_list); ++i) {
        nfa_t* cur = concat_list[i];
        nfa_node_add_transition(ret->accepting_state, NFA_TRANS_EPS, cur->initial_state);
        ret->accepting_state = cur->accepting_state;

        for (int j = 0; j < da_size(cur->nodes); ++j) {
            da_append(&ret->nodes, cur->nodes[j]);
        }
        nfa_deinit(cur);
        free(cur);
    }
    return ret;
}

void nfa_from_regex_handle_regop(regex_symbol_t* symbol, nfa_t*** union_list, nfa_t*** concat_list) {
    // This is where the magic in Mc-Naughton–Yamada–Thompson really happens
    char op = symbol->value_0;
    switch(op) {
        case '*':
            {
                assert(da_size(*concat_list) > 0);
                nfa_t* last = (*concat_list)[da_size(*concat_list) - 1];
                nfa_node_t* kleene_enter = malloc(sizeof(nfa_node_t));
                nfa_node_init(kleene_enter);
                nfa_node_t* kleene_exit  = malloc(sizeof(nfa_node_t));
                nfa_node_init(kleene_exit);

                da_append(&last->nodes, kleene_enter);
                da_append(&last->nodes, kleene_exit);

                nfa_node_add_transition(kleene_enter, NFA_TRANS_EPS, last->initial_state);
                nfa_node_add_transition(kleene_enter, NFA_TRANS_EPS, kleene_exit);
                nfa_node_add_transition(last->accepting_state, NFA_TRANS_EPS, last->initial_state);
                nfa_node_add_transition(last->accepting_state, NFA_TRANS_EPS, kleene_exit);

                last->initial_state = kleene_enter;
                last->accepting_state = kleene_exit;
            }
            break;
        case '+':
            {
                assert(da_size(*concat_list) > 0);
                // Almost like kleene star, but no kleene_enter node
                nfa_t* last = (*concat_list)[da_size(*concat_list) - 1];
                nfa_node_t* plus_exit = malloc(sizeof(nfa_node_t));
                nfa_node_init(plus_exit);

                da_append(&last->nodes, plus_exit);

                nfa_node_add_transition(last->accepting_state, NFA_TRANS_EPS, last->initial_state);
                nfa_node_add_transition(last->accepting_state, NFA_TRANS_EPS, plus_exit);

                last->accepting_state = plus_exit;
            }
            break;
        case '|':
            {
                assert(da_size(*concat_list) > 0);
                // TODO: handle (|...). Empty string nfa
                nfa_t* merged = concat_nfas(*concat_list);
                da_append(union_list, merged);
                da_clear(concat_list);
            }
            break;
        case '?':
            {
                assert(da_size(*concat_list) > 0);
                nfa_t* last = (*concat_list)[da_size(*concat_list) - 1];

                nfa_node_t* quest_exit = malloc(sizeof(nfa_node_t));
                nfa_node_init(quest_exit);
                da_append(&last->nodes, quest_exit);

                // Question mark is simply an epsilon bypassing the subexpression
                nfa_node_add_transition(last->accepting_state, NFA_TRANS_EPS, quest_exit);
                nfa_node_add_transition(last->initial_state, NFA_TRANS_EPS,   quest_exit);

                last->accepting_state = quest_exit;
            }
            break;
        default:
            fprintf(stderr, "Unknown regop %c\n", op);
            exit(1);
            break;
    }
}

void nfa_from_regex_impl(regex_t regex, size_t start, size_t end, nfa_t* nfa) {
    // Trying to implement the Mc-Naughton–Yamada–Thompson algorithm
    // a.k.a. a pretty obvious method of going from regex to nfa.

    // List of expressions that are supposed to be unioned together
    nfa_t** union_list = da_init(nfa_t*);
    // List of expressions that are supposed to be concatenated
    nfa_t** concat_list = da_init(nfa_t*);

    for (int i = start; i < end; ) {
        regex_symbol_t symbol = regex_symbol_at(&regex, i);
        switch (symbol.symbol_type) {
            case CLASS:
                {
                    nfa_node_t* node_a = malloc(sizeof(nfa_node_t));
                    nfa_node_init(node_a);
                    nfa_node_t* node_b = malloc(sizeof(nfa_node_t));
                    nfa_node_init(node_b);

                    for (int c = 0; c < 256; ++c) {
                        if (class_has_char(&symbol, c))
                            nfa_node_add_transition(node_a, (char)c, node_b);
                    }
                    nfa_t* nfa = malloc(sizeof(nfa_t));
                    nfa_init(nfa);
                    da_append(&nfa->nodes, node_a);
                    da_append(&nfa->nodes, node_b);

                    nfa->initial_state = node_a;
                    nfa->accepting_state = node_b;

                    da_append(&concat_list, nfa);
                    ++i;
                }
                break;
            case LPAREN:
                {
                    // TODO: handle stuff like (), empty nfa?
                    int loc = symbol.value_0;
                    nfa_t* sub_nfa = malloc(sizeof(nfa_t));
                    nfa_init(sub_nfa);
                    nfa_from_regex_impl(regex, i + 1, loc, sub_nfa);
                    da_append(&concat_list, sub_nfa);
                    i = loc + 1;
                }
                break;
            case RPAREN:
                {
                    assert(0); // Regex is malformed
                }
                break;
            case REGOP:
                {
                    nfa_from_regex_handle_regop(&symbol, &union_list, &concat_list);
                    ++i;
                }
                break;
        }
    }

    if (da_size(concat_list) > 0) {
        nfa_t* merged = concat_nfas(concat_list);
        da_append(&union_list, merged);
    }

    assert(da_size(union_list) > 0);

    nfa_t* ret = union_list[0];
    for (int i = 1; i < da_size(union_list); ++i) {
        nfa_t* cur = union_list[i];
        nfa_node_t* union_enter = malloc(sizeof(nfa_node_t));
        nfa_node_init(union_enter);
        nfa_node_t* union_exit = malloc(sizeof(nfa_node_t));
        nfa_node_init(union_exit);

        // A | B: Two eps going to each path, joining with eps
        //   INIT---EPS-->NFA_A---EPS-->FINAL
        //     \                        /
        //      ----EPS-->NFA_B---EPS---
        nfa_node_add_transition(union_enter, NFA_TRANS_EPS, ret->initial_state);
        nfa_node_add_transition(union_enter, NFA_TRANS_EPS, cur->initial_state);
        nfa_node_add_transition(ret->accepting_state, NFA_TRANS_EPS, union_exit);
        nfa_node_add_transition(cur->accepting_state, NFA_TRANS_EPS, union_exit);

        da_append(&ret->nodes, union_enter);
        da_append(&ret->nodes, union_exit);

        for (int j = 0; j < da_size(cur->nodes); ++j) {
            da_append(&ret->nodes, cur->nodes[j]);
        }
        nfa_deinit(cur);
        free(cur);

        ret->initial_state = union_enter;
        ret->accepting_state = union_exit;
    }

    da_deinit(&concat_list);
    da_deinit(&union_list);

    // Move ret into nfa
    // TODO: how to make the interface the same while avoiding this weird a.f. stuff
    nfa_deinit(nfa);

    nfa->initial_state   = ret->initial_state;
    nfa->accepting_state = ret->accepting_state;
    nfa->nodes           = ret->nodes;

    free(ret);
}

void nfa_from_regex(char* regex_string, size_t size, nfa_t* nfa) {
    regex_t regex;
    regex_init(&regex);
    regex_preprocess(regex_string, size, &regex);
    nfa_from_regex_impl(regex, 0, da_size(regex.symbols), nfa);
    regex_deinit(&regex);
}
