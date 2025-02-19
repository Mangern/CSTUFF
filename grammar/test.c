#include "lex.h"
#include "lr.h"

int main() {
    char* text = "x + y";

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

    // T -> ( E )
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(T),
        3, 
        SYMBOL_TERM(LPAREN),
        SYMBOL_NONTERM(E),
        SYMBOL_TERM(RPAREN)
    );

    // T -> Z
    grammar_add_rule(&grammar, 
        SYMBOL_NONTERM(T),
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

    grammar_debug_print(stdout, &grammar);

    grammar_deinit(&grammar);

    return 0;
}
