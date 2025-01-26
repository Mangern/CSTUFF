#include "da.h"
#include "preprocess.h"

#include <string.h>
#include <stdio.h>

void debug_regex_symbol(regex_symbol_t symbol) {
    switch (symbol.symbol_type) {
        case REGOP:
            printf("[REGOP]    %c", (char)symbol.value_0);
            break;
        case CLASS:
            printf("[CLASS]    %lx %lx %lx %lx", symbol.value_0, symbol.value_1, symbol.value_2, symbol.value_3);
            break;
        case LPAREN:
            printf("[LPAREN] %3lu", symbol.value_0);
            break;
        case RPAREN:
            printf("[RPAREN] %3lu", symbol.value_0);
            break;

    }

    printf(" %lu\n", symbol.value_0);
}

int main() {
    regex_t regex;
    char* regex_str = "(-|\\+)*\\d+";

    regex_preprocess(regex_str, strlen(regex_str), &regex);

    for (int i = 0; i < regex.symbols->size; ++i) {
        regex_symbol_t* symbol = da_at(regex.symbols, i);
        printf("%3d ", i);
        debug_regex_symbol(*symbol);
    }

    regex_deinit(&regex);
}
