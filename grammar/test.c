#include "lex.h"
#include "lr.h"

#include <string.h>

int main() {
    grammar_t grammar;
    grammar_init(&grammar);

    // E' -> E
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(E_PRIME),
        1, 
        SYMBOL_NONTERM(E)
    );

    // E -> E + T
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(E),
        3, 
        SYMBOL_NONTERM(E),
        SYMBOL_TERM(PLUS),
        SYMBOL_NONTERM(T)
    );

    // E -> E - T
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(E),
        3, 
        SYMBOL_NONTERM(E),
        SYMBOL_TERM(MINUS),
        SYMBOL_NONTERM(T)
    );

    // E -> T
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(E),
        1, 
        SYMBOL_NONTERM(T)
    );

    //T -> F
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(T),
        1,
        SYMBOL_NONTERM(F)
    );

    //F -> ( E )
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(F),
        3,
        SYMBOL_TERM(LPAREN),
        SYMBOL_NONTERM(E),
        SYMBOL_TERM(RPAREN)
    );

    //F -> Z * F
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(F),
        3,
        SYMBOL_NONTERM(Z),
        SYMBOL_TERM(STAR),
        SYMBOL_NONTERM(F)
    );

    //F -> Z
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(F),
        1,
        SYMBOL_NONTERM(Z)
    );

    // Z -> x
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(Z),
        1,
        SYMBOL_TERM(X)
    );

    // Z -> y
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(Z),
        1,
        SYMBOL_TERM(Y)
    );


    grammar_construct_itemsets(&grammar);
    grammar_construct_slr(&grammar);

    grammar_debug_print(stdout, &grammar);

    char* str_to_parse = "x + y * (x + y)";
    grammar_parse(&grammar, str_to_parse, strlen(str_to_parse));


    grammar_deinit(&grammar);

    return 0;
}
