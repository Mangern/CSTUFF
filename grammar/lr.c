#include "lr.h"
#include "lex.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

void grammar_init(grammar_t* grammar) {
    da_init(&(grammar->rules), sizeof(rule_t));
    da_init(&grammar->itemsets, sizeof(grammar_itemset_t));
    // To be populated later
    grammar->table_action = NULL;
    grammar->table_goto = NULL;
}

void grammar_itemset_init(grammar_itemset_t* itemset) {
    itemset->items = malloc(sizeof(da_t));
    da_init(itemset->items, sizeof(grammar_item_t));
}

int item_cmp(const void* data_item_a, const void* data_item_b) {
    grammar_item_t* item_a = (grammar_item_t*)data_item_a;
    grammar_item_t* item_b = (grammar_item_t*)data_item_b;
    if (item_a->rule_idx < item_b->rule_idx) return -1;
    if (item_a->rule_idx > item_b->rule_idx) return 1;
    if (item_a->dot_pos < item_b->dot_pos) return -1;
    if (item_a->dot_pos > item_b->dot_pos) return 1;
    return 0;
}

bool grammar_itemsets_equal(grammar_itemset_t* itemset_a, grammar_itemset_t* itemset_b) {
    if (itemset_a->items->size != itemset_b->items->size) return false;

    // TODO: better to keep sorted as an invariant for itemset
    da_sort(itemset_a->items, item_cmp);
    da_sort(itemset_b->items, item_cmp);

    for (size_t i = 0; i < itemset_a->items->size; ++i) {
        if (item_cmp(da_at(itemset_a->items, i), da_at(itemset_b->items, i))) return false;
    }
    return true;
}

void grammar_itemset_copy(grammar_itemset_t* itemset_dest, grammar_itemset_t* itemset_src) {
    da_clear(itemset_dest->items);
    da_resize(itemset_dest->items, itemset_src->items->size);
    memcpy(itemset_dest->items->buffer, itemset_src->items->buffer, itemset_src->items->size * itemset_src->items->elem_size);
}

void grammar_add_rule(grammar_t* grammar, symbol_t start_symbol, size_t n_results, ...) {
    va_list args;
    va_start(args, n_results);

    rule_t rule;
    rule.start_symbol = start_symbol;
    da_init(&rule.result_symbols, sizeof(symbol_t));

    for (size_t i = 0; i < n_results; ++i) {
        symbol_t prod_symbol = va_arg(args, symbol_t);
        da_push_back(&rule.result_symbols, &prod_symbol);
    }

    da_push_back(&grammar->rules, &rule);

    va_end(args);
}

rule_t grammar_rule_at(grammar_t* grammar, size_t index) {
    return *(rule_t *)da_at(&grammar->rules, index);
}

void grammar_deinit(grammar_t* grammar) {
    for (size_t i = 0; i < grammar->rules.size; ++i) {
        rule_t* rule = da_at(&grammar->rules, i);
        da_deinit(&rule->result_symbols);
    }
    da_deinit(&(grammar->rules));

    for (size_t i = 0; i < grammar->itemsets.size; ++i) {
        grammar_itemset_deinit(da_at(&grammar->itemsets, i));
    }
    da_deinit(&(grammar->itemsets));

    if (grammar->table_goto != NULL) {
        for (size_t i = 0; i < grammar->itemsets.size; ++i) {
            free(grammar->table_goto[i]);
        }
        free(grammar->table_goto);
    }

    if (grammar->table_action != NULL) {
        for (size_t i = 0; i < grammar->itemsets.size; ++i) {
            free(grammar->table_action[i]);
        }
        free(grammar->table_action);
    }
}

void grammar_itemset_deinit(grammar_itemset_t *itemset) {
    da_deinit(itemset->items);
    free(itemset->items);
}

char* grammar_symbol_str(symbol_t symbol) {
    if (symbol.is_terminal) {
        return token_str(symbol.symbol_val);
    }

    NonTerminal nt = symbol.symbol_val;
    switch (nt) {
        case E_PRIME:
            return "E'";
        case E:
            return "E";
        case T:
            return "T";
        case F:
            return "F";
        case Z:
            return "Z";
        case NUM_NONTERMINAL:
            return "NUM_NONTERMINAL";
        case EPS:
            return "EPS";
    }
    fprintf(stderr, "Unexpected symbol value %d in grammar_symbol_str\n", nt);
}

/**
 * Compute the closure of an itemset.
 * Use it to recover the full set of items from the kernel items.
 */
grammar_itemset_t itemset_closure(grammar_t* grammar, grammar_itemset_t* itemset) {
    bool added[NUM_SYMBOLS];
    memset(added, 0, NUM_SYMBOLS * sizeof(bool));
    bool added_this_round = true;
    grammar_itemset_t result;
    grammar_itemset_init(&result);
    grammar_itemset_copy(&result, itemset);

    while (added_this_round) {
        added_this_round = false;
        for (size_t i = 0; i < result.items->size; ++i) {
            // process items of the form A -> * . B *
            // where B is non-terminal
            grammar_item_t item = *(grammar_item_t*)da_at(result.items, i);
            rule_t rule = grammar_rule_at(grammar, item.rule_idx);

            if (item.dot_pos >= rule.result_symbols.size) continue;

            symbol_t symbol_after_dot = *(symbol_t *)da_at(&rule.result_symbols, item.dot_pos);
            if (symbol_after_dot.is_terminal) continue;

            size_t symbol_after_dot_index = grammar_symbol_index(symbol_after_dot);
            if (added[symbol_after_dot_index]) continue;

            // Add items B -> . *
            for (size_t j = 0; j < grammar->rules.size; ++j) {
                rule_t candidate_rule = grammar_rule_at(grammar, j);
                if (grammar_symbol_index(candidate_rule.start_symbol) != symbol_after_dot_index) continue;

                grammar_item_t item_to_add = (grammar_item_t) {
                    .dot_pos = 0,
                    .rule_idx = j
                };
                da_push_back(result.items, &item_to_add);
            }
            added[symbol_after_dot_index] = true;
            added_this_round = true;
        }
    }

    return result;
}

/*
 * GOTO(itemset, symbol) = (closure of) set of items acquired by traversing the dot 
 * over symbol in the productions of the itemset.
 * To save space, don't compute closure until needed.
 *
 * // A -> * . X * ---> A -> * X . *
 *
 * Gives caller ownership over returned itemset (needs to be deinitted at some point).
 */
grammar_itemset_t itemset_goto(grammar_t* grammar, grammar_itemset_t* itemset, size_t symbol_idx) {
    grammar_itemset_t result;
    grammar_itemset_init(&result);

    grammar_itemset_t itemset_full = itemset_closure(grammar, itemset);

    for (size_t i = 0; i < itemset_full.items->size; ++i) {
        grammar_item_t item = *(grammar_item_t*)da_at(itemset_full.items, i);
        rule_t item_rule = grammar_rule_at(grammar, item.rule_idx);
        if (item.dot_pos >= item_rule.result_symbols.size) continue;
        size_t compare_symbol_idx = grammar_symbol_index(
            *(symbol_t*)da_at(&item_rule.result_symbols, item.dot_pos)
        );
        if (compare_symbol_idx != symbol_idx) continue;

        grammar_item_t derived_item;
        // Same rule, advanced dot.
        derived_item.rule_idx = item.rule_idx;
        derived_item.dot_pos = item.dot_pos + 1;
        da_push_back(result.items, &derived_item);
    }

    grammar_itemset_deinit(&itemset_full);
    return result;
}

// Maybe grow the goto table
void grammar_make_goto_rows(grammar_t* grammar, size_t goal_rows) {
    if (goal_rows <= grammar->ngoto) return;

    grammar->table_goto = realloc(grammar->table_goto, goal_rows * sizeof(int*));
    for (size_t i = grammar->ngoto; i < goal_rows; ++i) {
        grammar->table_goto[i] = malloc(NUM_SYMBOLS * sizeof(int));
        memset(grammar->table_goto[i], -1, NUM_SYMBOLS * sizeof(int));
    }
    grammar->ngoto = goal_rows;
}

void grammar_construct_itemsets(grammar_t* grammar) {
    grammar_itemset_t initial_set;
    grammar_itemset_init(&initial_set);

    grammar_item_t initial_item;
    initial_item.dot_pos = 0;
    for (initial_item.rule_idx = 0; initial_item.rule_idx < grammar->rules.size; ++initial_item.rule_idx) {
        if (grammar_symbol_index(grammar_rule_at(grammar, initial_item.rule_idx).start_symbol) == SYMBOL_START)
            break;
    }

    da_push_back(initial_set.items, &initial_item);

    da_push_back(&grammar->itemsets, &initial_set);

    bool added_this_round = true;

    grammar->table_goto = malloc(0);
    grammar->ngoto = 0;
    grammar_make_goto_rows(grammar, 1);

    while (added_this_round) {
        added_this_round = false;
        for (size_t i = 0; i < grammar->itemsets.size; ++i) {
            for (size_t symbol_idx = 0; symbol_idx < NUM_SYMBOLS; ++symbol_idx) {
                grammar_itemset_t* itemset_curr = da_at(&grammar->itemsets, i);
                grammar_itemset_t itemset_derived = itemset_goto(grammar, itemset_curr, symbol_idx);

                // Check if derived exists already
                int exists = -1;
                for (size_t j = 0; j < grammar->itemsets.size; ++j) {
                    grammar_itemset_t* itemset_existing = da_at(&grammar->itemsets, j);
                    if (grammar_itemsets_equal(&itemset_derived, itemset_existing)) {
                        exists = j;
                        break;
                    }
                }

                if (exists != -1) {
                    grammar_make_goto_rows(grammar, i + 1);
                    grammar->table_goto[i][symbol_idx] = exists;
                }

                if (exists != -1 || itemset_derived.items->size == 0) {
                    // This itemset will not be remembered
                    grammar_itemset_deinit(&itemset_derived);
                    continue;
                }

                grammar_make_goto_rows(grammar, grammar->itemsets.size + 1);
                grammar->table_goto[i][symbol_idx] = grammar->itemsets.size;
                da_push_back(&grammar->itemsets, &itemset_derived);
                added_this_round = true;
            }
        }
    }

    assert(grammar->ngoto >= grammar->itemsets.size);
}

/*
 * set_first: allocated NUM_SYMBOLS x NUM_SYMBOLS array of bool.
 */
void grammar_compute_first(grammar_t* grammar, bool set_first[NUM_SYMBOLS][NUM_SYMBOLS]) {
    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        memset(set_first[i], 0, NUM_SYMBOLS * sizeof(bool));
    }
    for (int i = 0; i < NUM_TOKENS; ++i) {
        size_t symbol_idx = grammar_symbol_index(SYMBOL_TERM(i));
        set_first[symbol_idx][symbol_idx] = true;
    }

    size_t symbol_eps_idx = grammar_symbol_index(SYMBOL_NONTERM(EPS));
    set_first[symbol_eps_idx][symbol_eps_idx] = true;

    bool added_this_round = true;
    bool to_add[NUM_SYMBOLS];

    while (added_this_round) {
        added_this_round = false;

        for (size_t k = 0; k < grammar->rules.size; ++k) {
            rule_t rule = grammar_rule_at(grammar, k);
            size_t nonterm_symbol_idx = grammar_symbol_index(rule.start_symbol);

            memset(to_add, 0, NUM_SYMBOLS * sizeof(bool));
            // Production X -> Y_1 Y_2 ... 
            //
            // FIRST(Y_i) is added to FIRST(X) if i == 1 or all before i are nullable (has EPS in FIRST).
            for (size_t j = 0; j < rule.result_symbols.size; ++j) {
                size_t result_symbol_idx = grammar_symbol_index(*(symbol_t*)da_at(&rule.result_symbols, j));
                // Union FIRST for all elements in production
                for (size_t p = 0; p < NUM_SYMBOLS; ++p) {
                    to_add[p] |= set_first[result_symbol_idx][p];
                }

                // If an element in a production is not nullable, the remaining 
                // elements do not contribute to FIRST of this production
                if (!set_first[result_symbol_idx][symbol_eps_idx]) break;
            }

            // Union the accumulated bitset
            for (size_t p = 0; p < NUM_SYMBOLS; ++p) {
                if (to_add[p] == 1 && set_first[nonterm_symbol_idx][p] == 0)
                    added_this_round = true;

                set_first[nonterm_symbol_idx][p] |= to_add[p];
            }
        }
    }
}

/*
 * Compute FIRST(prod_seq[start_idx] prod_seq[start_idx+1] ...)
 * That is FIRST of a concatenation of symbols
 *
 * prod_seq: da_t<symbol_t>
 */
void grammar_sequence_first(da_t* prod_seq, size_t start_idx, bool set_first[NUM_SYMBOLS][NUM_SYMBOLS], bool result_first[NUM_SYMBOLS]) {
    bool to_add[NUM_SYMBOLS];
    size_t i;
    size_t symbol_eps_idx = grammar_symbol_index(SYMBOL_NONTERM(EPS));
    memset(result_first, 0, NUM_SYMBOLS * sizeof(bool));

    for (i = start_idx; i < prod_seq->size; ++i) {
        size_t result_symbol_idx = grammar_symbol_index(*(symbol_t*)da_at(prod_seq, i));
        for (size_t p = 0; p < NUM_SYMBOLS; ++p) {
            if (p == symbol_eps_idx) continue;
            result_first[p] |= set_first[result_symbol_idx][p];
        }
        if (!set_first[result_symbol_idx][symbol_eps_idx]) break;
    }

    if (i == prod_seq->size) {
        // EPS was in all result rules.
        result_first[symbol_eps_idx] = true;
    }
}

void grammar_compute_follow(grammar_t *grammar, bool set_first[NUM_SYMBOLS][NUM_SYMBOLS], bool set_follow[NUM_SYMBOLS][NUM_SYMBOLS]) {
    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        memset(set_follow[i], 0, NUM_SYMBOLS * sizeof(bool));
    }

    // EOF always follows start symbol
    set_follow[grammar_symbol_index(SYMBOL_NONTERM(E_PRIME))][grammar_symbol_index(SYMBOL_TERM(EOF_T))] = true;
    size_t symbol_eps_idx = grammar_symbol_index(SYMBOL_NONTERM(EPS));

    bool added_this_round = true;

    while (added_this_round) {
        added_this_round = false;

        for (size_t rule_idx = 0; rule_idx < grammar->rules.size; ++rule_idx) {
            rule_t rule = grammar_rule_at(grammar, rule_idx);

            size_t start_symbol_idx = grammar_symbol_index(rule.start_symbol);

            // A -> alpha B beta
            for (size_t i = 0; i < rule.result_symbols.size; ++i) {
                size_t result_symbol_idx = grammar_symbol_index(*(symbol_t*)da_at(&rule.result_symbols, i));
                bool add_start_symbol_follow = (i == rule.result_symbols.size - 1);
                if (i < rule.result_symbols.size - 1) {
                    bool first_rest[NUM_SYMBOLS];
                    // Compute FIRST(beta)
                    grammar_sequence_first(&rule.result_symbols, i + 1, set_first, first_rest);

                    // everything in FIRST(beta) except eps should be in FOLLOW(B)
                    for (size_t p = 0; p < NUM_SYMBOLS; ++p) {
                        if (p == symbol_eps_idx) continue;
                        if (set_follow[result_symbol_idx][p] == false && first_rest[p] == true)
                            added_this_round = true;
                        set_follow[result_symbol_idx][p] |= first_rest[p];
                    }

                    // if FIRST(beta) contains EPS, everything in FOLLOW(A) is in FOLLOW(B)
                    if (first_rest[symbol_eps_idx]) {
                        add_start_symbol_follow = true;
                    }
                }

                if (add_start_symbol_follow) {
                    for (size_t p = 0; p < NUM_SYMBOLS; ++p) {
                        if (set_follow[result_symbol_idx][p] == false && set_follow[start_symbol_idx][p] == true)
                            added_this_round = true;
                        set_follow[result_symbol_idx][p] |= set_follow[start_symbol_idx][p];
                    }
                }
            }
        }
    }
}

void grammar_construct_slr(grammar_t *grammar) {
    bool set_first[NUM_SYMBOLS][NUM_SYMBOLS];
    bool set_follow[NUM_SYMBOLS][NUM_SYMBOLS];

    grammar_compute_first(grammar, set_first);
    grammar_compute_follow(grammar, set_first, set_follow);

    size_t num_states = grammar->itemsets.size;
    grammar->table_action = malloc(num_states * sizeof(action_t*));

    for (size_t i = 0; i < num_states; ++i) {
        grammar->table_action[i] = malloc(NUM_SYMBOLS * sizeof(action_t));
        for (size_t j = 0; j < NUM_SYMBOLS; ++j) {
            grammar->table_action[i][j].type = UNDEFINED;
        }
    }

    // Fill ACTION table
    for (size_t i = 0; i < num_states; ++i) {
        grammar_itemset_t itemset = itemset_closure(grammar, (grammar_itemset_t*)da_at(&grammar->itemsets, i));

        for (size_t item_idx = 0; item_idx < itemset.items->size; ++item_idx) {
            grammar_item_t *item = da_at(itemset.items, item_idx);
            rule_t rule = grammar_rule_at(grammar, item->rule_idx);
            size_t start_idx = grammar_symbol_index(rule.start_symbol);
            if (item->dot_pos >= rule.result_symbols.size) {
                // Ends with dot
                if (start_idx == grammar_symbol_index(SYMBOL_NONTERM(SYMBOL_START))) {
                    grammar->table_action[i][grammar_symbol_index(SYMBOL_TERM(EOF_T))].type = ACCEPT;
                } else {
                    for (size_t p = 0; p < NUM_SYMBOLS; ++p) {
                        if (!set_follow[start_idx][p]) continue;
                        if (grammar->table_action[i][p].type == SHIFT) {
                            // Conflict
                            fprintf(stderr, "Shift/reduce conflict. ACTION[%zu, %s].", i, grammar_symbol_str(grammar_symbol_from_index(p)));
                            exit(1);
                        }
                        grammar->table_action[i][p].type = REDUCE;
                        grammar->table_action[i][p].argument = item->rule_idx;
                    }
                }
                continue;
            } 

            symbol_t symbol_after_dot = *(symbol_t *)da_at(&rule.result_symbols, item->dot_pos);
            if (!symbol_after_dot.is_terminal) continue;
            size_t symbol_idx_after_dot = grammar_symbol_index(symbol_after_dot);
            int next_state = grammar->table_goto[i][symbol_idx_after_dot];
            if (next_state == -1) {
                fprintf(stderr, "Failed to create action for state %zu. Has no GOTO on symbol %s", i, grammar_symbol_str(symbol_after_dot));
                exit(1);
            }

            if (grammar->table_action[i][symbol_idx_after_dot].type == REDUCE) {
                fprintf(stderr, "Shift/reduce conflict. ACTION[%zu, %s]", i, grammar_symbol_str(symbol_after_dot));
                exit(1);
            }

            grammar->table_action[i][symbol_idx_after_dot].type = SHIFT;
            grammar->table_action[i][symbol_idx_after_dot].argument = next_state;
        }

        grammar_itemset_deinit(&itemset);
    }
}

size_t grammar_symbol_index(symbol_t symbol) {
    if (symbol.is_terminal) {
        return NUM_NONTERMINAL + symbol.symbol_val;
    }
    return symbol.symbol_val;
}

symbol_t grammar_symbol_from_index(size_t index) {
    if (index < NUM_NONTERMINAL) return SYMBOL_NONTERM(index);
    return SYMBOL_TERM(index - NUM_NONTERMINAL);
}

void grammar_print_rule(FILE* out, grammar_t* grammar, size_t rule_idx) {
    rule_t* rule = da_at(&grammar->rules, rule_idx);
    fprintf(out, "%s ->", grammar_symbol_str(rule->start_symbol));

    for (size_t j = 0; j < rule->result_symbols.size; ++j) {
        symbol_t* sym = da_at(&rule->result_symbols, j);
        fprintf(out, " %s", grammar_symbol_str(*sym));
    }
}

void grammar_debug_print(FILE* out, grammar_t* grammar) {
    fprintf(out, "===== RULES =====\n");
    fprintf(out, "Number of rules: %zu\n", grammar->rules.size);

    for (size_t i = 0; i < grammar->rules.size; ++i) {
        grammar_print_rule(out, grammar, i);
        fprintf(out, "\n");
    }
    fprintf(out, "\n");
    fprintf(out, "===== ITEMSETS =====\n");

    for (size_t i = 0; i < grammar->itemsets.size; ++i) {
        fprintf(out, "Itemset %02zu:\n", i);
        grammar_itemset_t itemset = itemset_closure(grammar, (grammar_itemset_t*)da_at(&grammar->itemsets, i));
        for (size_t j = 0; j < itemset.items->size; ++j) {
            grammar_item_t item = *(grammar_item_t*)da_at(itemset.items, j);
            rule_t rule = grammar_rule_at(grammar, item.rule_idx);
            fprintf(out, "  %s ->", grammar_symbol_str(rule.start_symbol));

            for (size_t j = 0; j < rule.result_symbols.size; ++j) {
                symbol_t* sym = da_at(&rule.result_symbols, j);
                if (j == item.dot_pos) {
                    fprintf(out, " DOT");
                }
                fprintf(out, " %s", grammar_symbol_str(*sym));
            }
            if (item.dot_pos == rule.result_symbols.size) 
                fprintf(out, " DOT");

            fprintf(out, "\n");
        }
        grammar_itemset_deinit(&itemset);
    }


    bool set_first[NUM_SYMBOLS][NUM_SYMBOLS];
    bool set_follow[NUM_SYMBOLS][NUM_SYMBOLS];

    grammar_compute_first(grammar, set_first);

    fprintf(out, "\n");
    fprintf(out, "===== FIRST =====\n");

    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        fprintf(out, "%s:", grammar_symbol_str(grammar_symbol_from_index(i)));
        for (int j = 0; j < NUM_SYMBOLS; ++j) {
            if (set_first[i][j]) {
                fprintf(out, " %s", grammar_symbol_str(grammar_symbol_from_index(j)));
            }
        }
        fprintf(out, "\n");
    }

    grammar_compute_follow(grammar, set_first, set_follow);

    fprintf(out, "\n");
    fprintf(out, "===== FOLLOW =====\n");
    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        fprintf(out, "%s:", grammar_symbol_str(grammar_symbol_from_index(i)));
        for (int j = 0; j < NUM_SYMBOLS; ++j) {
            if (set_follow[i][j]) {
                fprintf(out, " %s", grammar_symbol_str(grammar_symbol_from_index(j)));
            }
        }
        fprintf(out, "\n");
    }

    fprintf(out, "\n");

    if (grammar->table_action != NULL) {
        for (size_t i = 0; i < grammar->itemsets.size; ++i) {
            for (int j = 0; j < NUM_SYMBOLS; ++j) {
                ActionType type = grammar->table_action[i][j].type;
                size_t arg = grammar->table_action[i][j].argument;
                if (type == UNDEFINED) continue;
                fprintf(out, "ACTION[%zu, %s] = ", i, grammar_symbol_str(grammar_symbol_from_index(j)));
                if (type == SHIFT) {
                    fprintf(out, "shift %zu\n", arg);
                } else if (type == REDUCE) {
                    fprintf(out, "reduce ");
                    grammar_print_rule(out, grammar, arg);
                    fprintf(out, "\n");
                } else {
                    fprintf(out, "accept\n");
                }
            }
        }
    }
}

void grammar_parse(grammar_t *grammar, char *str, size_t length) {
    lexer_t lexer;
    lexer_init(&lexer, str, length);

    size_t state = 0;
    da_t stack;
    da_init(&stack, sizeof(size_t));
    da_push_back(&stack, &state);

    Token curr_symbol = lexer_advance(&lexer);
    printf("Input: %s\nRules:\n", str);
    for (;;) {
        assert(stack.size > 0);
        size_t stack_top = *(size_t*)da_at(&stack, stack.size - 1);
        size_t curr_symbol_idx = grammar_symbol_index(SYMBOL_TERM(curr_symbol));

        action_t action = grammar->table_action[stack_top][curr_symbol_idx];

        switch (action.type) {
            case UNDEFINED:
            {
                printf("REJECT\n");
                return;
            }
            break;
            case SHIFT:
            {
                size_t next_state = grammar->table_action[stack_top][curr_symbol_idx].argument;
                da_push_back(&stack, &next_state);
                // Only advance on shift
                curr_symbol = lexer_advance(&lexer);
            }
            break;
            case REDUCE:
            {
                size_t rule_idx = grammar->table_action[stack_top][curr_symbol_idx].argument;
                rule_t rule = grammar_rule_at(grammar, rule_idx);
                assert(stack.size > rule.result_symbols.size);
                // Pop the prod symbols
                da_resize(&stack, stack.size - rule.result_symbols.size);
                stack_top = *(size_t*)da_at(&stack, stack.size - 1);
                int next_state = grammar->table_goto[stack_top][grammar_symbol_index(rule.start_symbol)];
                grammar_print_rule(stdout, grammar, rule_idx);
                puts("");
                if (next_state == -1) {
                    printf("REJECT\n");
                    return;
                }

                size_t next_state_ss = next_state;
                da_push_back(&stack, &next_state_ss);
                // do not advance input
            }
            break;
            case ACCEPT:
            {
                printf("ACCEPT\n");
                return;
            }
            break;
        }
    }

    da_deinit(&stack);
}
