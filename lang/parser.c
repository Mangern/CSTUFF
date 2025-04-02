#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "parser.h"
#include "da.h"
#include "lex.h"
#include "tree.h"

static void fail(token_t);
static void fail_expected(token_t, token_type_t);
static token_t peek_expect_advance(token_type_t);
static node_t* parse_global_statement();
static node_t* parse_variable_declaration();
static node_t* parse_function_declaration();
static node_t* parse_arg_list();
static node_t* parse_block();
static node_t* parse_expression();
static node_t* parse_function_call(node_t*);
static operator_t parse_operator_str(char* operator_str, bool);

void parse() {
    // program : global global global global
    // global  : variable declaration | function declaration
    // variable declaration : TYPE IDENTIFIER ;
    root = node_create(LIST);

    node_t* node;
    for (;;) {
        node = parse_global_statement();
        if (!node) break;
        da_append(&root->children, node);
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

    fail(token);
    // unreachable
    assert(false && "Unreachable");
}

// Note: does not parse trailing ';'
static node_t* parse_variable_declaration() {
    // Advance through typename
    token_t token = peek_expect_advance(LEX_TYPENAME);

    node_t* ret = node_create(VARIABLE_DECLARATION);
    node_t* typename_node = node_create(TYPENAME);
    typename_node->data.typename_str = lexer_substring(token.begin_offset, token.end_offset);
    da_append(&ret->children, typename_node);

    token = peek_expect_advance(LEX_IDENTIFIER);

    node_t* child = node_create(IDENTIFIER);
    child->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);
    da_append(&ret->children, child);

    token = lexer_peek();

    if (token.type == LEX_WALRUS) {
        lexer_advance();
        da_append(&ret->children, parse_expression());
    }

    return ret;
}

static node_t* parse_function_declaration() {
    token_t token = lexer_peek();
    node_t* ret = node_create(FUNCTION_DECLARATION);
    node_t* identifier = node_create(IDENTIFIER);
    identifier->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);
    da_append(&ret->children, identifier);
    lexer_advance();

    token = lexer_peek();
    if (token.type != LEX_WALRUS) 
        fail_expected(token, LEX_WALRUS);
    lexer_advance();


    da_append(&ret->children, parse_arg_list());

    peek_expect_advance(LEX_ARROW);
    token = peek_expect_advance(LEX_TYPENAME);

    node_t* typename_node = node_create(TYPENAME);
    typename_node->data.typename_str = lexer_substring(token.begin_offset, token.end_offset);

    da_append(&ret->children, typename_node);

    token = lexer_peek();
    if (token.type != LEX_LBRACE)
        fail_expected(token, LEX_LBRACE);

    // parse body
    da_append(&ret->children, parse_block());

    peek_expect_advance(LEX_RBRACE);
    return ret;
}

static node_t* parse_arg_list() {
    token_t token = peek_expect_advance(LEX_LPAREN);

    node_t* ret = node_create(LIST);
    token = lexer_peek();

    while (token.type != LEX_RPAREN) {
        da_append(&ret->children, parse_variable_declaration());
        token = lexer_peek();
        if (token.type == LEX_RPAREN) break;
        if (token.type != LEX_COMMA)
            fail_expected(token, LEX_COMMA);
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
                da_append(&return_node->children, parse_expression());
            }
            da_append(&block_node->children, return_node);
            peek_expect_advance(LEX_SEMICOLON);
        } else if (token.type == LEX_TYPENAME) {
            // <TYPENAME> <DECLARATION> ';' -> VARIABLE_DECLARATION
            da_append(&block_node->children, parse_variable_declaration());
            peek_expect_advance(LEX_SEMICOLON);

        } else if (token.type == LEX_LBRACE) {
            // '{' ... '}' -> BLOCK[...]
            da_append(&block_node->children, parse_block());
            peek_expect_advance(LEX_RBRACE);
        } else if (token.type == LEX_IDENTIFIER) {
            lexer_advance();

            node_t* identifier_node = node_create(IDENTIFIER);
            identifier_node->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);

            token = lexer_peek();

            // TODO: assignment
            if (token.type == LEX_LPAREN) {
                // FUNCTION_CALL[...]
                da_append(&block_node->children, parse_function_call(identifier_node));
                peek_expect_advance(LEX_SEMICOLON);
            } else {
                fail_expected(token, LEX_LPAREN);
            }
        } else {
            fail(token);
        }
    }

    return block_node;
}

static bool has_precedence(operator_t operator_1, operator_t operator_2) {
    if (operator_1 == BINARY_MUL || operator_1 == BINARY_DIV) {
        return operator_2 != BINARY_MUL && operator_2 != BINARY_DIV;
    }
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
        da_append(&operator_node->children, lhs);
        da_append(&operator_node->children, rhs);
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
        da_append(&operator_node->children, rhs);

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

static node_t* parse_expression() {
    // Expression: Numeric_literal
    // Expression: Identifier
    // Expression: ( Expression )
    // Expression: Expression + Expression
    token_t token = lexer_peek();
    if (token.type == LEX_IDENTIFIER) {
        node_t* node = node_create(IDENTIFIER);
        node->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);
        lexer_advance();
        token = lexer_peek();

        if (token.type == LEX_LPAREN) {
            node = parse_function_call(node);
            token = lexer_peek();
        }

        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, node);
        } else if (token.type == LEX_SEMICOLON || token.type == LEX_RPAREN || token.type == LEX_COMMA) {
            return node;
        } else {
            fail(token);
        }
    } else if (token.type == LEX_INTEGER || token.type == LEX_REAL || token.type == LEX_STRING) {
        node_t* node = node_create(INTEGER_LITERAL);
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
        } else {
            assert(false && "Not implemented");
        }
        lexer_advance();

        token = lexer_peek();

        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, node);
        } else if (token.type == LEX_SEMICOLON || token.type == LEX_RPAREN || token.type == LEX_COMMA) {
            return node;
        } else {
            fail(token);
        }
    } else if (token.type == LEX_LPAREN) {
        lexer_advance();
        node_t* expr = parse_expression();
        peek_expect_advance(LEX_RPAREN);

        node_t* ret = node_create(PARENTHESIZED_EXPRESSION);
        da_append(&ret->children, expr);

        token = lexer_peek();

        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, ret);
        } else if (token.type == LEX_SEMICOLON || token.type == LEX_RPAREN) {
            return ret;
        } else {
            fail(token);
        }
    } else if (token.type == LEX_OPERATOR) {
        lexer_advance();

        node_t* expr = parse_expression();

        return merge_unary_op(token, expr);
    } else {
        fail(token);
    }
    assert(false && "Unreachable");
}

// Expects identifier node to be consumed and given to us
static node_t* parse_function_call(node_t* identifier_node) {
    token_t token = peek_expect_advance(LEX_LPAREN);

    node_t* ret = node_create(FUNCTION_CALL);
    da_append(&ret->children, identifier_node);

    node_t* list_node = node_create(LIST);

    token = lexer_peek();
    while (token.type != LEX_RPAREN) {
        da_append(&list_node->children, parse_expression());
        token = lexer_peek();
        if (token.type == LEX_RPAREN) break;
        token = peek_expect_advance(LEX_COMMA);
    }
    lexer_advance();

    da_append(&ret->children, list_node);
    return ret;
}

static token_t peek_expect_advance(token_type_t expected) {
    token_t token = lexer_peek();
    if (token.type != expected)
        fail_expected(token, expected);
    lexer_advance();
    return token;
}

static void fail_expected(token_t token, token_type_t expected) {
    location_t loc;
    loc = lexer_offset_location(token.begin_offset);
    fprintf(stderr, "%s:%d:%d: Expected '%s', got '%s'\n", CURRENT_FILE_NAME, loc.line+1, loc.character+1, TOKEN_TYPE_NAMES[expected], TOKEN_TYPE_NAMES[token.type]);
    exit(1);
}

static void fail(token_t token) {
    location_t loc;
    loc = lexer_offset_location(token.begin_offset);
    fprintf(stderr, "%s:%d:%d: Unexpected token '%s'\n", CURRENT_FILE_NAME, loc.line+1, loc.character+1, TOKEN_TYPE_NAMES[token.type]);
    exit(1);
}
