# A parser generator

Generate an SLR(1) parser.

## Example

Consider the grammar with terminals `x, y, +, -, *, (, )` and productions

```
E' -> E
E -> E + T
E -> E - T
E -> T
T -> F
F -> ( E )
F -> F * F
F -> Z
Z -> x
Z -> y
```

where `E'` is the start state.

```c
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

// ... more rules


char* str_to_parse = "x + y * (x + y)";
grammar_parse(&grammar, str_to_parse, strlen(str_to_parse));
```

Output:
```
Input: x + y * (x + y)
Rules:
Z -> x
F -> Z
T -> F
E -> T
Z -> y
Z -> x
F -> Z
T -> F
E -> T
Z -> y
F -> Z
T -> F
E -> E + T
F -> ( E )
F -> Z * F
T -> F
E -> E + T
ACCEPT
```
Where the reversed rules are steps to follow to derive the input from the grammar, choosing the rightmost derivation at each step.

The generator will abort if it detects that the given grammar is not SLR(1) parseable.
