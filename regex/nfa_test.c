#include "da.h"
#include "nfa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFA_PRINT_TEST(nfa, str, result) { int ret = nfa_accepts(nfa, str, strlen(str)); printf("\"%s\" %s match %s %s \x1b[0m\n", str, result == 0 ? "should not" : "should", ret == result ? "and it\x1b[1;32m" : "but it\x1b[1;31m" , ret == 0 ? "doesn't" : "does"); }

void debug_nfa(nfa_t* nfa) {
    printf("# Nodes: %zu\n", da_size(nfa->nodes));

    printf("Initial: %p\n", nfa->initial_state);
    printf("Final  : %p\n", nfa->accepting_state);
    for (int i = 0; i < da_size(nfa->nodes); ++i) {
        nfa_node_t* cur = nfa->nodes[i];
        for (int j = 0; j < da_size(cur->transitions); ++j) {
            transition_t trans = cur->transitions[j];
            if (trans.c == NFA_TRANS_EPS) {
                printf("%p ->EPS %p\n", cur, trans.next_node);
            } else {
                printf("%p ->%c   %p\n", cur, trans.c, trans.next_node);
            }
        }
    }
}

void nfa_union_test() {
    printf("===== a|b =====\n");
    char* regex = "a|b";
    nfa_t nfa;
    nfa_init(&nfa);
    nfa_from_regex(regex, strlen(regex), &nfa);

    NFA_PRINT_TEST(&nfa, "a", 1);
    NFA_PRINT_TEST(&nfa, "b", 1);
    NFA_PRINT_TEST(&nfa, "c", 0);
    NFA_PRINT_TEST(&nfa, "ab", 0);

    printf("\n\n\n");
    nfa_free_nodes(&nfa);
    nfa_deinit(&nfa);
}

void nfa_kleene_test() {
    printf("===== a*ba =====\n");
    char* regex = "a*ba";
    nfa_t nfa;
    nfa_init(&nfa);
    nfa_from_regex(regex, strlen(regex), &nfa);

    NFA_PRINT_TEST(&nfa, "a", 0);
    NFA_PRINT_TEST(&nfa, "b", 0);
    NFA_PRINT_TEST(&nfa, "aaba", 1);
    NFA_PRINT_TEST(&nfa, "aaaba", 1);
    NFA_PRINT_TEST(&nfa, "abba", 0);
    NFA_PRINT_TEST(&nfa, "ba", 1);
    NFA_PRINT_TEST(&nfa, "baba", 0);

    printf("\n\n\n");
    nfa_free_nodes(&nfa);
    nfa_deinit(&nfa);
}

void nfa_paren_test() {
    printf("===== (a|b)*c =====\n");

    char* regex = "(a|b)*c";
    nfa_t nfa;
    nfa_init(&nfa);
    nfa_from_regex(regex, strlen(regex), &nfa);

    NFA_PRINT_TEST(&nfa, "a", 0);
    NFA_PRINT_TEST(&nfa, "ac", 1);
    NFA_PRINT_TEST(&nfa, "abc", 1);
    NFA_PRINT_TEST(&nfa, "cc", 0);
    NFA_PRINT_TEST(&nfa, "bbc", 1);
    NFA_PRINT_TEST(&nfa, "babac", 1);
    NFA_PRINT_TEST(&nfa, "bcbac", 0);

    printf("\n\n\n");
    nfa_free_nodes(&nfa);
    nfa_deinit(&nfa);
}

void nfa_question_test() {
    printf("===== c?(ab)*a? =====\n");
    char* regex = "c?(ab)*a?";
    nfa_t nfa;
    nfa_init(&nfa);
    nfa_from_regex(regex, strlen(regex), &nfa);

    NFA_PRINT_TEST(&nfa, "a", 1);
    NFA_PRINT_TEST(&nfa, "c", 1);
    NFA_PRINT_TEST(&nfa, "ca", 1);
    NFA_PRINT_TEST(&nfa, "cababa", 1);
    NFA_PRINT_TEST(&nfa, "cabaa", 0);
    NFA_PRINT_TEST(&nfa, "ccab", 0);
    NFA_PRINT_TEST(&nfa, "ababa", 1);
    NFA_PRINT_TEST(&nfa, "", 1);

    printf("\n\n\n");
    nfa_free_nodes(&nfa);
    nfa_deinit(&nfa);
}

void nfa_plus_test() {
    printf("===== a+c?b+ =====\n");
    char* regex = "a+c?b+";
    nfa_t nfa;
    nfa_init(&nfa);
    nfa_from_regex(regex, strlen(regex), &nfa);

    NFA_PRINT_TEST(&nfa, "a", 0);
    NFA_PRINT_TEST(&nfa, "ab", 1);
    NFA_PRINT_TEST(&nfa, "b", 0);
    NFA_PRINT_TEST(&nfa, "aaacbbb", 1);
    NFA_PRINT_TEST(&nfa, "aaabbb", 1);
    NFA_PRINT_TEST(&nfa, "aaabbba", 0);
    NFA_PRINT_TEST(&nfa, "aaaccbbb", 0);
    NFA_PRINT_TEST(&nfa, "cbbb", 0);

    printf("\n\n\n");
    nfa_free_nodes(&nfa);
    nfa_deinit(&nfa);
}

int main() {
    nfa_union_test();
    nfa_kleene_test();
    nfa_paren_test();
    nfa_question_test();
    nfa_plus_test();
    return 0;
}
