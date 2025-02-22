#include "dfa.h"
#include "nfa.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define DFA_PRINT_TEST(dfa, str, result) { int ret = dfa_accepts(dfa, str, strlen(str)); printf("\"%s\" %s match %s %s \x1b[0m\n", str, result == 0 ? "should not" : "should", ret == result ? "and it\x1b[1;32m" : "but it\x1b[1;31m" , ret == 0 ? "doesn't" : "does"); }

#define WALLTIME(t) ((double)(t).tv_sec + 1e-6 * (double)(t).tv_usec)

void debug_nfa(nfa_t* nfa) {
    printf("# Nodes: %zu\n", nfa->nodes->size);

    printf("Initial: %p\n", nfa->initial_state);
    printf("Final  : %p\n", nfa->accepting_state);
    for (int i = 0; i < nfa->nodes->size; ++i) {
        nfa_node_t* cur = *(nfa_node_t**)da_at(nfa->nodes, i);
        for (int j = 0; j < cur->transitions->size; ++j) {
            transition_t trans = *(transition_t*)da_at(cur->transitions, j);
            if (trans.c == NFA_TRANS_EPS) {
                printf("%p ->EPS %p\n", cur, trans.next_node);
            } else {
                printf("%p ->%c   %p\n", cur, trans.c, trans.next_node);
            }
        }
    }
}

void debug_dfa(dfa_t* dfa) {
    printf("# nodes: %d\n", dfa->num_nodes);

    for (int i = 1; i < dfa->num_nodes; ++i) {
        for (int c = 0; c < 256; ++c) {
            if (dfa->trans[i][c] != 0) {
                printf("%d %d %c\n", i, dfa->trans[i][c], c);
            }
        }
    }
    printf("Final:");
    for (int i = 0; i < dfa->num_nodes; ++i) {
        if (dfa->node_flag[i] & DFA_FLAG_ACCEPT)printf(" %d", i);
    }
    printf("\n");
}

void dfa_concat_test() {
    char* regex = "aabcdefg";
    printf("===== %s =====\n", regex);
    dfa_t* dfa = dfa_from_regex(regex, strlen(regex));

    printf("Num nodes: %d\n\n", dfa->num_nodes);

    DFA_PRINT_TEST(dfa, "aabcdefg", 1);
    DFA_PRINT_TEST(dfa, "abcdefg", 0);
    DFA_PRINT_TEST(dfa, "aabc", 0);
    DFA_PRINT_TEST(dfa, "a", 0);

    dfa_deinit(dfa);
    free(dfa);

    printf("\n\n\n");
}

void dfa_paren_test() {
    char* regex = "(abc)((ab)c)(a)";
    printf("===== %s =====\n", regex);
    dfa_t* dfa = dfa_from_regex(regex, strlen(regex));

    printf("Num nodes: %d\n\n", dfa->num_nodes);

    DFA_PRINT_TEST(dfa, "abcabca", 1);
    DFA_PRINT_TEST(dfa, "abcaba", 0);
    DFA_PRINT_TEST(dfa, "ababca", 0);
    DFA_PRINT_TEST(dfa, "abcabcabc", 0);

    dfa_deinit(dfa);
    free(dfa);

    printf("\n\n\n");
}

void dfa_kleene_test() {
    char* regex = "a*bbc*";
    printf("===== %s =====\n", regex);
    dfa_t* dfa = dfa_from_regex(regex, strlen(regex));
    printf("Num nodes: %d\n\n", dfa->num_nodes);

    DFA_PRINT_TEST(dfa, "bbc", 1);
    DFA_PRINT_TEST(dfa, "aaaabbc", 1);
    DFA_PRINT_TEST(dfa, "aabb", 1);
    DFA_PRINT_TEST(dfa, "bb", 1);
    DFA_PRINT_TEST(dfa, "aaaaabcccc", 0);
    DFA_PRINT_TEST(dfa, "aaaaaaaaabbcccccccc", 1);
    DFA_PRINT_TEST(dfa, "aaaaaaaaacccccccc", 0);
    DFA_PRINT_TEST(dfa, "aaabbcca", 0);
    DFA_PRINT_TEST(dfa, "aaabbccbb", 0);

    dfa_deinit(dfa);
    free(dfa);

    printf("\n\n\n");
}

void dfa_plus_test() {
    char* regex = "(ab)+c*(ba)+";

    printf("===== %s =====\n", regex);
    dfa_t* dfa = dfa_from_regex(regex, strlen(regex));
    printf("Num nodes: %d\n\n", dfa->num_nodes);

    DFA_PRINT_TEST(dfa, "ccc", 0);
    DFA_PRINT_TEST(dfa, "abcba", 1);
    DFA_PRINT_TEST(dfa, "ababccc", 0);
    DFA_PRINT_TEST(dfa, "ababcccbaba", 1);
    DFA_PRINT_TEST(dfa, "ababba", 1);
    DFA_PRINT_TEST(dfa, "abababab", 0);
    DFA_PRINT_TEST(dfa, "ababababcccccccccbabababa", 1);
    DFA_PRINT_TEST(dfa, "cababccbaba", 0);
    DFA_PRINT_TEST(dfa, "ab", 0);

    dfa_deinit(dfa);
    free(dfa);

    printf("\n\n\n");
}

void dfa_union_test() {
    char* regex = "c?(a|b)*(d|c)+";

    printf("===== %s =====\n", regex);
    dfa_t* dfa = dfa_from_regex(regex, strlen(regex));
    printf("Num nodes: %d\n\n", dfa->num_nodes);

    DFA_PRINT_TEST(dfa, "ac", 1);
    DFA_PRINT_TEST(dfa, "cac", 1);
    DFA_PRINT_TEST(dfa, "cababccc", 1);
    DFA_PRINT_TEST(dfa, "aaaddd", 1);
    DFA_PRINT_TEST(dfa, "cababa", 0);
    DFA_PRINT_TEST(dfa, "abababdddccc", 1);
    DFA_PRINT_TEST(dfa, "cbbbbdddc", 1);
    DFA_PRINT_TEST(dfa, "dddccc", 1);
    DFA_PRINT_TEST(dfa, "ddddccca", 0);

    dfa_deinit(dfa);
    free(dfa);

    printf("\n\n\n");
}

void dfa_tdt4205_test() {
    char* regex = "(((ab)*a?)|((ba)*b?))cc*";
    printf("===== %s =====\n", regex);
    dfa_t* dfa = dfa_from_regex(regex, strlen(regex));
    printf("Num nodes: %d\n\n", dfa->num_nodes);

    
    DFA_PRINT_TEST(dfa, "c", 1);
    DFA_PRINT_TEST(dfa, "cccccc", 1);
    DFA_PRINT_TEST(dfa, "", 0);
    DFA_PRINT_TEST(dfa, "ac", 1);
    DFA_PRINT_TEST(dfa, "bc", 1);
    DFA_PRINT_TEST(dfa, "abc", 1);
    DFA_PRINT_TEST(dfa, "bac", 1);
    DFA_PRINT_TEST(dfa, "abac", 1);
    DFA_PRINT_TEST(dfa, "a", 0);
    DFA_PRINT_TEST(dfa, "b", 0);
    DFA_PRINT_TEST(dfa, "babcccccc", 1);
    DFA_PRINT_TEST(dfa, "ababcccc", 1);
    DFA_PRINT_TEST(dfa, "babababac", 1);
    DFA_PRINT_TEST(dfa, "ababababcccccccc", 1);
    DFA_PRINT_TEST(dfa, "babababccccc", 1);
    DFA_PRINT_TEST(dfa, "ababababac", 1);
    DFA_PRINT_TEST(dfa, "bbabac", 0);
    DFA_PRINT_TEST(dfa, "aac", 0);
    DFA_PRINT_TEST(dfa, "bbc", 0);
    DFA_PRINT_TEST(dfa, "baac", 0);
    DFA_PRINT_TEST(dfa, "baabc", 0);
    DFA_PRINT_TEST(dfa, "abbc", 0);
    DFA_PRINT_TEST(dfa, "abbac", 0);
    DFA_PRINT_TEST(dfa, "abcabcccc", 0);
    DFA_PRINT_TEST(dfa, "bababbac", 0);
    DFA_PRINT_TEST(dfa, "bacccaccc", 0);
    DFA_PRINT_TEST(dfa, "abababaacccccc", 0);
    DFA_PRINT_TEST(dfa, "aababc", 0);
    DFA_PRINT_TEST(dfa, "abababa", 0);
    DFA_PRINT_TEST(dfa, "bababababab", 0);

    dfa_deinit(dfa);
    free(dfa);

}

void dfa_example_test() {
    char* regex = "a(b|c)*";
    printf("===== %s =====\n", regex);
    dfa_t* dfa = dfa_from_regex(regex, strlen(regex));
    printf("Num nodes: %d\n\n", dfa->num_nodes);

    DFA_PRINT_TEST(dfa, "abbccbb", 1);
    DFA_PRINT_TEST(dfa, "bbccbb", 0);
    DFA_PRINT_TEST(dfa, "a", 1);
    DFA_PRINT_TEST(dfa, "aa", 0);
    DFA_PRINT_TEST(dfa, "0000", 0);
    DFA_PRINT_TEST(dfa, "abbbccbbccbcccccc", 1);
    DFA_PRINT_TEST(dfa, "abbbccbbccbcccccca", 0);

    dfa_deinit(dfa);
    free(dfa);

    printf("\n\n\n");
}

float time_dfa_init(char* regex) {
    size_t size = strlen(regex);
    struct timeval t_start, t_end;

    const int NUM_ITERATIONS = 1000;

    float total_time_secs = 0.0;


    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        gettimeofday ( &t_start, NULL );
        dfa_t* dfa = dfa_from_regex(regex, size);
        gettimeofday ( &t_end, NULL );

        total_time_secs += WALLTIME(t_end) - WALLTIME(t_start);

        dfa_deinit(dfa);
        free(dfa);
    }

    float avg_time_ms = total_time_secs / (float)NUM_ITERATIONS * 1000.f;
    return avg_time_ms;
}

void dfa_fat_test() {
    char* regex = "((((a|b)+(c|d)+)|((c|d)+(a|b)+))*z+x?)*((oo|io*i)p*)*|((((a|b)+(c|d)+)|((c|d)+(a|b|c|d)+))*z+x?)*((oo|yo*y)p*)*|((((a|b)+(c|d)+)|((c|d|a|b|o|p)+(a|b)+))*z+x?)*((oo|uu*i)p*)*";
    printf("Initializing dfa with %zu character regex 1000 times...\n", strlen(regex));
    float avg_time_ms = time_dfa_init(regex);
    printf("Average time to initialize: %f ms\n", avg_time_ms);
    printf("Time per character: %f ms\n", avg_time_ms / (float)strlen(regex));
    printf("\n\n\n");

}

void dfa_linear_test() {
    char* regexes[12];
    regexes[0]  = "(a|b)";
    regexes[1]  = "(a|b)*cd*e";
    regexes[2]  = "((a|b)*cd*)*|ef";
    regexes[3]  = "((a|b)*cd*)*(a|ef)*z";
    regexes[4]  = "((a|b)*cd*)*(a|ef*)*z|b|a";
    regexes[5]  = "(a)b|((a|b)*cd*)*(a|ef*)*z|b|a";
    regexes[6]  = "(b*)b|((a|b)*cd*)*(a|ef*)*z|(b|a)+u";
    regexes[7]  = "(c*)b|((a|b)*cd*)*(a|ef*)*z|(b|a)+u?(ac)";
    regexes[8]  = "(d*)b?|((a|b)*cd*)*(a|ef*)*z|(b|a)+u?(a|c)?b*";
    regexes[9]  = "(e*)b?|((a|b)*cd*)*(a|ef*)*z|(b|a)+u?(a|c)b*|(ab)*";
    regexes[10] = "(f*)b?|((a|b)*cd*)*(a|ef*)*z|(b|a)+u?(a|c)b*|(ab)*|(a*)b?|((a|b)*cd?)*(a|ef*)*8|(y|p)+i?(x|z)b*|cab*";
    regexes[11] = "(z*)u*|((a+i)*hz*)*(2|13*)*4|(z|a)*u+(a+c)b*|(oc)*|(a*)b?|((a|b)*cd?)*(a|hf*)*8|(y|p)+i?(x|z)b*|cab*|(a*)b?|((u|i)*cd*)*(a|bf*)*z|(k|l)?u+(h|4)b*|(p2)*|(a*)b?|((a|b)*cd?)*(a|ef*)*8|(y|p)+i?(u|o)b*|cb*";

    for (int i = 0; i < 12; ++i) {
        float avg_time_ms = time_dfa_init(regexes[i]);
        float time_per_char = avg_time_ms / (float)strlen(regexes[i]);
        unsigned long len = strlen(regexes[i]);
        printf("Regex length: %3zu chars. %.5f ms per init, %.5f ms per char.\n", len, avg_time_ms, time_per_char);
    }
}

int main(int argc, char** argv) {
    dfa_concat_test();
    dfa_paren_test();
    dfa_kleene_test();
    dfa_plus_test();
    dfa_union_test();
    dfa_tdt4205_test();
    dfa_example_test();
    dfa_fat_test();
    dfa_linear_test();
}
