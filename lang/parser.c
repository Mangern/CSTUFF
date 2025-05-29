#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "parser.h"
#include "da.h"
#include "fail.h"
#include "lex.h"
#include "tree.h"

static token_t peek_expect_advance(token_type_t);
static node_t* parse_global_statement();
static node_t* parse_variable_declaration();
static node_t* parse_function_declaration();
static node_t* parse_arg_list();
static node_t* parse_block();
static node_t* parse_typename();
static node_t* parse_expression();
static node_t* parse_function_call(node_t*);
static node_t* parse_cast();
static node_t* parse_assignment(node_t*);
static node_t* parse_array_indexing(node_t*);
static operator_t parse_operator_str(char* operator_str, bool);

void parse() {
    root = node_create(LIST);

    node_t* node;
    for (;;) {
        node = parse_global_statement();
        if (!node) break;
        da_append(root->children, node);
    }
}

static node_t* parse_global_statement() {
    token_t token = lexer_peek();

    if (token.type == LEX_TYPENAME) {
        node_t* node = parse_variable_declaration();
        peek_expect_advance(LEX_SEMICOLON);
        return node;
    } else if (token.type == LEX_IDENTIFIER) {
        return parse_function_declaration();
    } else if (token.type == LEX_END) {
        return NULL;
    }

    fail_token(token);
    // unreachable
    assert(false && "Unreachable");
}

// Note: does not parse trailing ';'
static node_t* parse_variable_declaration() {
    // Advance through typename
    node_t* typename_node = parse_typename();

    node_t* ret = node_create(VARIABLE_DECLARATION);
    da_append(ret->children, typename_node);

    token_t token = peek_expect_advance(LEX_IDENTIFIER);

    node_t* child = node_create_leaf(IDENTIFIER, token);
    child->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);

    da_append(ret->children, child);

    token = lexer_peek();

    if (token.type == LEX_WALRUS) {
        lexer_advance();
        da_append(ret->children, parse_expression());
    }

    return ret;
}

static node_t* parse_function_declaration() {
    token_t token = lexer_peek();
    if (token.type != LEX_IDENTIFIER) 
        fail_token_expected(token, LEX_IDENTIFIER);

    node_t* identifier = node_create_leaf(IDENTIFIER, token);
    identifier->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);

    node_t* ret = node_create(FUNCTION_DECLARATION);
    da_append(ret->children, identifier);
    lexer_advance();

    token = lexer_peek();
    if (token.type != LEX_WALRUS) 
        fail_token_expected(token, LEX_WALRUS);
    lexer_advance();


    da_append(ret->children, parse_arg_list());

    peek_expect_advance(LEX_ARROW);
    token = peek_expect_advance(LEX_TYPENAME);

    node_t* typename_node = node_create_leaf(TYPENAME, token);
    typename_node->data.typename_str = lexer_substring(token.begin_offset, token.end_offset);

    da_append(ret->children, typename_node);

    token = lexer_peek();
    if (token.type != LEX_LBRACE)
        fail_token_expected(token, LEX_LBRACE);

    // parse body
    node_t* block = parse_block();

    token_t rbrace_token = peek_expect_advance(LEX_RBRACE);

    // insert return statement if not exists
    if (da_size(block->children) == 0 || block->children[da_size(block->children)-1]->type != RETURN_STATEMENT) {
        node_t* fake_return_node = node_create(RETURN_STATEMENT);
        // TODO: check if setting rbrace as location makes sense
        fake_return_node->pos = rbrace_token;
        da_append(block->children, fake_return_node);
    }

    da_append(ret->children, block);
    return ret;
}

static node_t* parse_arg_list() {
    token_t token = peek_expect_advance(LEX_LPAREN);

    node_t* ret = node_create(LIST);
    token = lexer_peek();

    while (token.type != LEX_RPAREN) {
        da_append(ret->children, parse_variable_declaration());
        token = lexer_peek();
        if (token.type == LEX_RPAREN) break;
        if (token.type != LEX_COMMA)
            fail_token_expected(token, LEX_COMMA);
        lexer_advance();
    }
    lexer_advance();
    return ret;
}

static node_t* parse_block() {
    token_t token = peek_expect_advance(LEX_LBRACE);

    node_t* block_node = node_create(BLOCK);
    for (;;) {
        token = lexer_peek();
        if (token.type == LEX_RBRACE) break;

        if (token.type == LEX_RETURN) {
            // <RETURN> <EXPRESSION>? ';' -> RETURN_STATEMENT[expression]
            lexer_advance();
            node_t* return_node = node_create(RETURN_STATEMENT);
            if (lexer_peek().type != LEX_SEMICOLON) {
                da_append(return_node->children, parse_expression());
            }
            da_append(block_node->children, return_node);
            peek_expect_advance(LEX_SEMICOLON);
        } else if (token.type == LEX_TYPENAME) {
            // <TYPENAME> <DECLARATION> ';' -> VARIABLE_DECLARATION
            da_append(block_node->children, parse_variable_declaration());
            peek_expect_advance(LEX_SEMICOLON);

        } else if (token.type == LEX_LBRACE) {
            // '{' ... '}' -> BLOCK[...]
            da_append(block_node->children, parse_block());
            peek_expect_advance(LEX_RBRACE);
        } else if (token.type == LEX_IDENTIFIER) {

            node_t* identifier_node = node_create_leaf(IDENTIFIER, token);
            identifier_node->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);

            lexer_advance();

            token = lexer_peek();

            if (token.type == LEX_LPAREN) {
                // FUNCTION_CALL[...]
                da_append(block_node->children, parse_function_call(identifier_node));
                peek_expect_advance(LEX_SEMICOLON);
            } else if (token.type == LEX_WALRUS) {
                da_append(block_node->children, parse_assignment(identifier_node));
                peek_expect_advance(LEX_SEMICOLON);
            } else if (token.type == LEX_LBRACKET) {
                node_t* indexing_node = parse_array_indexing(identifier_node);
                token = lexer_peek();

                if (token.type == LEX_WALRUS) {
                    da_append(block_node->children, parse_assignment(indexing_node));
                    peek_expect_advance(LEX_SEMICOLON);
                } else if (token.type == LEX_SEMICOLON) {
                    lexer_advance();
                } else {
                    fail_token(token);
                }
            } else {
                fail_token(token);
            }
        } else if (token.type == LEX_IF) {
            // if ( EXPRESSION ) BLOCK 
            // if ( EXPRESSION ) BLOCK else BLOCK
            lexer_advance();
            node_t* if_node = node_create(IF_STATEMENT);

            peek_expect_advance(LEX_LPAREN);
            da_append(if_node->children, parse_expression());
            peek_expect_advance(LEX_RPAREN);

            da_append(if_node->children, parse_block());

            peek_expect_advance(LEX_RBRACE);

            token = lexer_peek();

            if (token.type == LEX_ELSE) {
                lexer_advance();

                da_append(if_node->children, parse_block());

                peek_expect_advance(LEX_RBRACE);
            }

            da_append(block_node->children, if_node);
        } else if (token.type == LEX_WHILE) {
            // while (expression) BLOCK
            lexer_advance();
            node_t* while_node = node_create(WHILE_STATEMENT);
            peek_expect_advance(LEX_LPAREN);
            da_append(while_node->children, parse_expression());
            peek_expect_advance(LEX_RPAREN);

            da_append(while_node->children, parse_block());

            peek_expect_advance(LEX_RBRACE);

            da_append(block_node->children, while_node);
        } else {
            fail_token(token);
        }
    }

    return block_node;
}

static node_t* parse_typename() {
    token_t token = peek_expect_advance(LEX_TYPENAME);

    node_t* typename_node = node_create_leaf(TYPENAME, token);
    typename_node->data.typename_str = lexer_substring(token.begin_offset, token.end_offset);

    // Array type.
    if (lexer_peek().type == LEX_LBRACKET) {
        lexer_advance();

        node_t* array_type_node = node_create(ARRAY_TYPE);
        da_append(array_type_node->children, typename_node);
        node_t* dim_list = node_create(LIST);

        for (;;) {
            token_t literal_token = peek_expect_advance(LEX_INTEGER);
            char* literal_str = lexer_substring(literal_token.begin_offset, literal_token.end_offset);
            node_t* literal_node = node_create_leaf(INTEGER_LITERAL, literal_token);
            literal_node->data.int_literal_value = atol(literal_str);
            da_append(dim_list->children, literal_node);
            free(literal_str);

            token_t nxt = lexer_peek();
            if (nxt.type == LEX_COMMA) {
                lexer_advance();
                continue;
            } else if (nxt.type == LEX_RBRACKET) {
                break;
            }
            fail_token(nxt);
        }
        peek_expect_advance(LEX_RBRACKET);

        da_append(array_type_node->children, dim_list);
        return array_type_node;
    }
    return typename_node;
}

static bool has_precedence(operator_t operator_1, operator_t operator_2) {
    if (operator_1 == BINARY_MUL || operator_1 == BINARY_DIV) {
        return operator_2 != BINARY_MUL && operator_2 != BINARY_DIV;
    }
    if (operator_2 == BINARY_MUL || operator_2 == BINARY_DIV)
        return false;

    if (operator_1 == BINARY_ADD || operator_1 == BINARY_SUB) {
        return operator_2 != BINARY_ADD && operator_2 != BINARY_SUB;
    }

    if (operator_2 == BINARY_ADD || operator_2 == BINARY_SUB)
        return false;

    return false;
}

// Find the correct place in `rhs` to insert `lhs`, preserving operator precedence.
static node_t* merge_subtrees(token_t operator_token, node_t* lhs, node_t* rhs) {
    char* lhs_operator_str = lexer_substring(operator_token.begin_offset, operator_token.end_offset);
    operator_t lhs_operator = parse_operator_str(lhs_operator_str, true);
    free(lhs_operator_str);

    if (rhs->type != OPERATOR || da_size(rhs->children) != 2 || has_precedence(rhs->data.operator, lhs_operator)) {
        node_t* operator_node = node_create(OPERATOR);
        operator_node->data.operator = lhs_operator;
        da_append(operator_node->children, lhs);
        da_append(operator_node->children, rhs);
        return operator_node;
    }


    rhs->children[0] = merge_subtrees(operator_token, lhs, rhs->children[0]);
    return rhs;
}

static operator_t parse_operator_str(char* operator_str, bool binary) {
    if (binary) {
        if (strcmp(operator_str, "+") == 0) {
            return BINARY_ADD;
        } else if (strcmp(operator_str, "-") == 0) {
            return BINARY_SUB;
        } else if (strcmp(operator_str, "*") == 0) {
            return BINARY_MUL;
        } else if (strcmp(operator_str, "/") == 0) {
            return BINARY_DIV;
        } else if (strcmp(operator_str, ">") == 0) {
            return BINARY_GT;
        } else if (strcmp(operator_str, "<") == 0) {
            return BINARY_LT;
        } else if (strcmp(operator_str, "==") == 0) {
            return BINARY_EQ;
        }
    } else {
        if (strcmp(operator_str, "-") == 0) {
            return UNARY_SUB;
        } else if (strcmp(operator_str, "!") == 0) {
            return UNARY_NEG;
        }
    }
    fprintf(stderr, "Parse operator str unhandled: %s, %d\n", operator_str, binary);
    exit(1);
    assert(false);
}

static node_t* merge_unary_op(token_t operator_token, node_t* rhs) {
    char* operator_str = lexer_substring(operator_token.begin_offset, operator_token.end_offset);
    operator_t operator = parse_operator_str(operator_str, false);
    free(operator_str);

    if (rhs->type != OPERATOR || da_size(rhs->children) != 2 || has_precedence(rhs->data.operator, operator)) {
        node_t* operator_node = node_create(OPERATOR);
        operator_node->data.operator = operator;
        da_append(operator_node->children, rhs);

        return operator_node;
    }

    rhs->children[0] = merge_unary_op(operator_token, rhs->children[0]);
    return rhs;
}

static node_t* expression_continuation(token_t operator_token, node_t* lhs) {
    // TODO: post operators
    node_t* rhs = parse_expression();

    return merge_subtrees(operator_token, lhs, rhs);
}

inline static bool token_is_expression_end(token_t token) {
    return token.type == LEX_SEMICOLON
        || token.type == LEX_COMMA
        || token.type == LEX_RPAREN
        || token.type == LEX_RBRACKET;
        
}

static node_t* parse_expression() {
    // Expression: Numeric_literal
    // Expression: Identifier
    // Expression: Array indexing
    // Expression: ( Expression )
    // Expression: Expression + Expression
    token_t token = lexer_peek();
    if (token.type == LEX_IDENTIFIER) {
        node_t* node = node_create_leaf(IDENTIFIER, token);
        node->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);
        lexer_advance();
        token = lexer_peek();

        if (token.type == LEX_LPAREN) {
            node = parse_function_call(node);
            token = lexer_peek();
        } else if (token.type == LEX_LBRACKET) {
            node = parse_array_indexing(node);
            token = lexer_peek();
        }

        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, node);
        } else if (token_is_expression_end(token)) {
            return node;
        } else {
            fail_token(token);
        }
    } else if (token.type == LEX_INTEGER || token.type == LEX_REAL || token.type == LEX_STRING || token.type == LEX_TRUE || token.type == LEX_FALSE) {
        node_t* node = node_create_leaf(INTEGER_LITERAL, token);
        // TODO: unnecessary malloc
        if (token.type == LEX_INTEGER) {
            char* literal_str = lexer_substring(token.begin_offset, token.end_offset);
            node->data.int_literal_value = atol(literal_str);
            free(literal_str);
        } else if (token.type == LEX_STRING) {
            node->type = STRING_LITERAL;
            node->data.string_literal_value = lexer_substring(token.begin_offset+1, token.end_offset-1);
        } else if (token.type == LEX_REAL) {
            node->type = REAL_LITERAL;
            char* literal_str = lexer_substring(token.begin_offset, token.end_offset);
            node->data.real_literal_value = atof(literal_str);
            free(literal_str);
        } else if (token.type == LEX_TRUE) {
            node->type = BOOL_LITERAL;
            node->data.bool_literal_value = true;
        } else if (token.type == LEX_FALSE) {
            node->type = BOOL_LITERAL;
            node->data.bool_literal_value = false;
        } else {
            assert(false && "Not implemented");
        }
        lexer_advance();

        token = lexer_peek();

        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, node);
        } else if (token_is_expression_end(token)) {
            return node;
        } else {
            fail_token(token);
        }
    } else if (token.type == LEX_LPAREN) {
        lexer_advance();
        node_t* expr = parse_expression();
        peek_expect_advance(LEX_RPAREN);

        node_t* ret = node_create(PARENTHESIZED_EXPRESSION);
        da_append(ret->children, expr);

        token = lexer_peek();

        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, ret);
        } else if (token_is_expression_end(token)) {
            return ret;
        } else {
            fail_token(token);
        }
    } else if (token.type == LEX_OPERATOR) {
        lexer_advance();

        node_t* expr = parse_expression();

        return merge_unary_op(token, expr);
    } else if (token.type == LEX_CAST) {
        lexer_advance();
        peek_expect_advance(LEX_LPAREN);
        node_t* cast_expr = parse_cast();
        peek_expect_advance(LEX_RPAREN);

        token = lexer_peek();

        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, cast_expr);
        } else if (token_is_expression_end(token)) {
            return cast_expr;
        } else {
            fail_token(token);
        }
    } else {
        fail_token(token);
    }
    assert(false && "Unreachable");
}

// Expects identifier node to be consumed and given to us
static node_t* parse_function_call(node_t* identifier_node) {
    token_t token = peek_expect_advance(LEX_LPAREN);

    node_t* ret = node_create(FUNCTION_CALL);
    da_append(ret->children, identifier_node);

    node_t* list_node = node_create(LIST);

    token = lexer_peek();
    while (token.type != LEX_RPAREN) {
        da_append(list_node->children, parse_expression());
        token = lexer_peek();
        if (token.type == LEX_RPAREN) break;
        token = peek_expect_advance(LEX_COMMA);
    }
    lexer_advance();

    da_append(ret->children, list_node);
    return ret;
}

static node_t* parse_cast() {
    // typename, 
    node_t* typename_node = parse_typename();
    peek_expect_advance(LEX_COMMA);
    node_t* expr_node = parse_expression();
    node_t* ret = node_create(CAST_EXPRESSION);
    da_append(ret->children, typename_node);
    da_append(ret->children, expr_node);
    return ret;
}

static node_t* parse_assignment(node_t* lhs_node) {
    peek_expect_advance(LEX_WALRUS);

    node_t* ret = node_create(ASSIGNMENT_STATEMENT);
    da_append(ret->children, lhs_node);
    da_append(ret->children, parse_expression());
    return ret;
}

static node_t* parse_array_indexing(node_t* identifier_node) {
    peek_expect_advance(LEX_LBRACKET);
    node_t* indexing = node_create(ARRAY_INDEXING);
    da_append(indexing->children, identifier_node);

    node_t* index_list = node_create(LIST);

    for (;;) {
        node_t* expr = parse_expression();
        da_append(index_list->children, expr);
        token_t token = lexer_peek();

        if (token.type == LEX_COMMA) {
            lexer_advance();
            continue;
        } else if (token.type == LEX_RBRACKET) 
            break;
        fail_token(token);
    }
    peek_expect_advance(LEX_RBRACKET);
    da_append(indexing->children, index_list);
    return indexing;
}

static token_t peek_expect_advance(token_type_t expected) {
    token_t token = lexer_peek();
    if (token.type != expected)
        fail_token_expected(token, expected);
    lexer_advance();
    return token;
}

