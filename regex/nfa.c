#include "nfa.h"
#include "da.h"
#include "preprocess.h"

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void nfa_init(nfa_t *nfa) {
    nfa->nodes = malloc(sizeof(da_t));
    da_init(nfa->nodes, sizeof(nfa_node_t*));
}

void nfa_deinit(nfa_t *nfa) {
    // NOTE: does not free up nodes. Only the local node pointer storage
    da_deinit(nfa->nodes);
    free(nfa->nodes);
}

void nfa_free_nodes(nfa_t *nfa) {
    // Separate from deinit, because sometimes nodes need to live longer
    // mindfuck lifetime business
    for (int i = 0; i < nfa->nodes->size; ++i) {
        nfa_node_t* node = *(nfa_node_t**)da_at(nfa->nodes, i);
        nfa_node_free(node);
    }
}

void nfa_node_free(nfa_node_t* node) {
    da_deinit(node->transitions);
    free(node->transitions);
    free(node);
}

void nfa_node_init(nfa_node_t* node) {
    node->transitions = malloc(sizeof(da_t));
    da_init(node->transitions, sizeof(transition_t));
}

void nfa_node_add_transition(nfa_node_t* node, char c, nfa_node_t* next) {
    transition_t trans = {c, next};
    da_push_back(node->transitions, &trans);
}



int nfa_accepts_impl(nfa_t* nfa, nfa_node_t* cur_state, char* string, size_t size) {
    if (size == 0) {
        if (nfa->accepting_state == cur_state) return 1;
    }

    int ret = 0;
    for (int i = 0; i < cur_state->transitions->size; ++i) {
        transition_t trans = *(transition_t *)da_at(cur_state->transitions, i);
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

void nfa_node_transition(nfa_node_t* node, char c, da_t* result) {
    for (int i = 0; i < node->transitions->size; ++i) {
        transition_t trans = *(transition_t*)da_at(node->transitions, i);
        if (trans.c == c) {
            da_push_back(result, &trans.next_node);
        }
    }
}

int* regex_paren_match(char* regex_string, size_t size) {
    int* paren_match = malloc(size * sizeof(int));
    memset(paren_match, -1, size * sizeof(int));
    da_t paren_stack;
    da_init(&paren_stack, sizeof(int));

    for (int i = 0; i < size; ++i) {
        if (regex_string[i] == '(') {
            da_push_back(&paren_stack, &i);
        } else if (regex_string[i] == ')') {
            assert(paren_stack.size > 0); // Else, unmatched rparen

            int loc = *(int*)da_at(&paren_stack, paren_stack.size - 1);
            paren_match[loc] = i;
            paren_match[i] = loc;
            da_pop_back(&paren_stack);
        }
    }


    assert(paren_stack.size == 0); // unmatched lparen
    da_deinit(&paren_stack);
    return paren_match;
}

nfa_t* concat_nfas(da_t* concat_list) {
    nfa_t* ret = *(nfa_t**)da_at(concat_list, 0);
    for (int i = 1; i < concat_list->size; ++i) {
        nfa_t* cur = *(nfa_t**)da_at(concat_list, i);
        nfa_node_add_transition(ret->accepting_state, NFA_TRANS_EPS, cur->initial_state);
        ret->accepting_state = cur->accepting_state;

        for (int j = 0; j < cur->nodes->size; ++j) {
            da_push_back(ret->nodes, (nfa_node_t**)da_at(cur->nodes, j));
        }
        nfa_deinit(cur);
        free(cur);
    }
    return ret;
}

void nfa_from_regex_handle_regop(regex_symbol_t* symbol, da_t* union_list, da_t* concat_list) {
    // This is where the magic in Mc-Naughton–Yamada–Thompson really happens
    char op = symbol->value_0;
    switch(op) {
        case '*':
            {
                assert(concat_list->size > 0);
                nfa_t* last = *(nfa_t**)da_at(concat_list, concat_list->size - 1);
                nfa_node_t* kleene_enter = malloc(sizeof(nfa_node_t));
                nfa_node_init(kleene_enter);
                nfa_node_t* kleene_exit  = malloc(sizeof(nfa_node_t));
                nfa_node_init(kleene_exit);

                da_push_back(last->nodes, &kleene_enter);
                da_push_back(last->nodes, &kleene_exit);

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
                assert(concat_list->size > 0);
                // Almost like kleene star, but no kleene_enter node
                nfa_t* last = *(nfa_t**)da_at(concat_list, concat_list->size - 1);
                nfa_node_t* plus_exit = malloc(sizeof(nfa_node_t));
                nfa_node_init(plus_exit);

                da_push_back(last->nodes, &plus_exit);

                nfa_node_add_transition(last->accepting_state, NFA_TRANS_EPS, last->initial_state);
                nfa_node_add_transition(last->accepting_state, NFA_TRANS_EPS, plus_exit);

                last->accepting_state = plus_exit;
            }
            break;
        case '|':
            {
                assert(concat_list->size > 0);
                // TODO: handle (|...). Empty string nfa
                nfa_t* merged = concat_nfas(concat_list);
                da_push_back(union_list, &merged);
                da_resize(concat_list, 0);
            }
            break;
        case '?':
            {
                assert(concat_list->size > 0);
                nfa_t* last = *(nfa_t**)da_at(concat_list, concat_list->size - 1);

                nfa_node_t* quest_exit = malloc(sizeof(nfa_node_t));
                nfa_node_init(quest_exit);
                da_push_back(last->nodes, &quest_exit);

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
    da_t union_list;
    da_init(&union_list, sizeof(nfa_t*));
    // List of expressions that are supposed to be concatenated
    da_t concat_list;
    da_init(&concat_list, sizeof(nfa_t*));

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
                    da_push_back(nfa->nodes, &node_a);
                    da_push_back(nfa->nodes, &node_b);

                    nfa->initial_state = node_a;
                    nfa->accepting_state = node_b;

                    da_push_back(&concat_list, &nfa);
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
                    da_push_back(&concat_list, &sub_nfa);
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

    if (concat_list.size > 0) {
        nfa_t* merged = concat_nfas(&concat_list);
        da_push_back(&union_list, &merged);
    }

    assert(union_list.size > 0);

    nfa_t* ret = *(nfa_t**)da_at(&union_list, 0);
    for (int i = 1; i < union_list.size; ++i) {
        nfa_t* cur = *(nfa_t**)da_at(&union_list, i);
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

        da_push_back(ret->nodes, &union_enter);
        da_push_back(ret->nodes, &union_exit);

        for (int j = 0; j < cur->nodes->size; ++j) {
            da_push_back(ret->nodes, (nfa_node_t**)da_at(cur->nodes, j));
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
    nfa_from_regex_impl(regex, 0, regex.symbols->size, nfa);
    regex_deinit(&regex);
}
