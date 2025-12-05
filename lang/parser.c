#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "parser.h"
#include "da.h"
#include "fail.h"
#include "lex.h"
#include "tree.h"
#include "type.h"

static token_t peek_expect_advance(token_type_t);
static node_t* parse_global_statement();
static node_t* parse_declaration_list();
static node_t* parse_type();
static node_t* parse_declaration(node_t*);
static node_t* parse_type_declaration();
static node_t* parse_block();
static node_t* parse_struct_body();
static node_t* parse_function_type();
static node_t* parse_expression();
static node_t* parse_function_call(node_t*);
static node_t* parse_cast();
static node_t* parse_assignment(node_t*);
static node_t* parse_block_operation(node_t*);
static node_t* parse_scope_resolution(node_t*);
static node_t* parse_dot_access(node_t*);
static node_t* parse_deref(node_t*);
static node_t* parse_array_indexing(node_t*);
static node_t* parse_alloc();
static operator_t parse_operator_str(char* operator_str, bool);

void parse() {
    root = node_create(LIST);

    node_t* node;
    for (;;) {
        node = parse_global_statement();
        if (!node) break;
        node_add_child(root, node);

        token_t token;
        for (;;) {
            token = lexer_peek();
            if (token.type == LEX_END) {
                return;
            } else if (token.type == LEX_SEMICOLON) {
                lexer_advance();
                continue;
            }
            break;
        }
    }
}

static node_t* parse_global_statement() {
    token_t token = lexer_peek();

    if (token.type == LEX_IDENTIFIER) {
        return parse_declaration(NULL);
    } else if (token.type == LEX_TYPE) {
        return parse_type_declaration();
    }

    fail_token(token);
    // unreachable
    assert(false && "Unreachable");
}

// If identifier is not NULL, expects IDENTIFIER token to be consumed
// FOLLOW: RPAREN, SEMICOLON
static node_t* parse_declaration(node_t* identifier) {
    // function name, assign the identifier
    token_t token;

    if (identifier == NULL) {
        token = peek_expect_advance(LEX_IDENTIFIER);

        identifier = node_create_leaf(IDENTIFIER, token);
        identifier->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);
    }

    peek_expect_advance(LEX_COLON);


    node_t* declaration = node_create(DECLARATION);
    node_add_child(declaration, identifier);

    token = lexer_peek();
    if (token.type == LEX_EQUAL) {
        lexer_advance();

        // type inference type situation

        token = lexer_peek();

        node_t* rhs;
        if (token.type == LEX_LBRACE) {
            // assign to function??
            rhs = parse_block();
            peek_expect_advance(LEX_RBRACE);
        } else {
            rhs = parse_expression();
        }

        node_add_child(declaration, rhs);

        return declaration;
    }

    node_t* type_node = parse_type();
    node_add_child(declaration, type_node);

    token = lexer_peek();

    if (token.type == LEX_SEMICOLON || token.type == LEX_RPAREN || token.type == LEX_COMMA) {
        return declaration;
    }

    peek_expect_advance(LEX_EQUAL);

    token = lexer_peek();

    node_t* rhs = 0;
    if (token.type == LEX_LBRACE) {
        rhs = parse_block();
        token_t rbrace_token = peek_expect_advance(LEX_RBRACE);
        // TODO: insert return statement if not exists
        if (da_size(rhs->children) == 0 || rhs->children[da_size(rhs->children)-1]->type != RETURN_STATEMENT) {
            node_t* fake_return_node = node_create_leaf(RETURN_STATEMENT, rbrace_token);
            // TODO: check if setting rbrace as location makes sense
            fake_return_node->pos = rbrace_token;
            node_add_child(rhs, fake_return_node);
        }
    } else {
        rhs = parse_expression();
    }
    
    node_add_child(declaration, rhs);

    return declaration;
}

static node_t* parse_type_declaration() {
    peek_expect_advance(LEX_TYPE);

    token_t token = peek_expect_advance(LEX_IDENTIFIER);
    node_t* identifier_node = node_create_leaf(IDENTIFIER, token);
    identifier_node->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);
    peek_expect_advance(LEX_EQUAL);
    node_t* type_node = parse_type();

    peek_expect_advance(LEX_SEMICOLON);
    node_t* type_decl_node = node_create(TYPE_DECLARATION);

    node_add_child(type_decl_node, identifier_node);
    node_add_child(type_decl_node, type_node);

    return type_decl_node;
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

            return_node->pos = token; // store keyword token

            if (lexer_peek().type != LEX_SEMICOLON) {
                node_add_child(return_node, parse_expression());
            }
            node_add_child(block_node, return_node);
            peek_expect_advance(LEX_SEMICOLON);
        } else if (token.type == LEX_LBRACE) {
            // '{' ... '}' -> BLOCK[...]
            node_add_child(block_node, parse_block());
            peek_expect_advance(LEX_RBRACE);
        // TODO:
        //} else if (token.type == LEX_TYPENAME) {
        //    // <TYPENAME> <DECLARATION> ';' -> VARIABLE_DECLARATION
        //    da_append(block_node->children, parse_variable_declaration());
        //    peek_expect_advance(LEX_SEMICOLON);

        } else if (token.type == LEX_IDENTIFIER) {
            node_t* identifier_node = node_create_leaf(IDENTIFIER, token);
            identifier_node->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);

            lexer_advance();

            token = lexer_peek();

            if (token.type == LEX_LPAREN) {
                // FUNCTION_CALL[...]
                node_add_child(block_node, parse_function_call(identifier_node));
                // TODO: other things than semi?
                peek_expect_advance(LEX_SEMICOLON);
            //} else if (token.type == LEX_WALRUS) {
            //    da_append(block_node->children, parse_assignment(identifier_node));
            //    peek_expect_advance(LEX_SEMICOLON);
            } else if (token.type == LEX_OPERATOR) {
                node_add_child(block_node, parse_block_operation(identifier_node));
                peek_expect_advance(LEX_SEMICOLON);
            } else if (token.type == LEX_LBRACKET) {
                node_t* indexing_node = parse_array_indexing(identifier_node);
                token = lexer_peek();

                if (token.type == LEX_EQUAL) {
                   node_add_child(block_node, parse_assignment(indexing_node));
                   peek_expect_advance(LEX_SEMICOLON);

                } else if (token.type == LEX_OPERATOR) {
                   node_add_child(block_node, parse_block_operation(indexing_node));
                   peek_expect_advance(LEX_SEMICOLON);

                } else if (token.type == LEX_SEMICOLON) {
                   lexer_advance();
                } else {
                   fail_token(token);
                }
            } else if (token.type == LEX_COLON) {
                node_add_child(block_node, parse_declaration(identifier_node));
                peek_expect_advance(LEX_SEMICOLON);
            } else if (token.type == LEX_EQUAL) {
                node_add_child(block_node, parse_assignment(identifier_node));
                peek_expect_advance(LEX_SEMICOLON);
            } else {
                fail_token(token);
            }
        } else if (token.type == LEX_IF) {
            // if ( EXPRESSION ) BLOCK 
            // if ( EXPRESSION ) BLOCK else BLOCK
            lexer_advance();
            node_t* if_node = node_create(IF_STATEMENT);

            if_node->pos = token; // store keyword token

            peek_expect_advance(LEX_LPAREN);
            node_add_child(if_node, parse_expression());
            peek_expect_advance(LEX_RPAREN);

            node_add_child(if_node, parse_block());

            peek_expect_advance(LEX_RBRACE);

            token = lexer_peek();

            if (token.type == LEX_ELSE) {
                lexer_advance();

                node_t* block_node = parse_block();
                block_node->pos = token; // store keyword token (ugly edition)

                node_add_child(if_node, block_node);

                peek_expect_advance(LEX_RBRACE);
            }

            node_add_child(block_node, if_node);
        } else if (token.type == LEX_WHILE) {
            // while (expression) BLOCK
            lexer_advance();
            node_t* while_node = node_create(WHILE_STATEMENT);

            while_node->pos = token; // store keyword token

            peek_expect_advance(LEX_LPAREN);
            node_add_child(while_node, parse_expression());
            peek_expect_advance(LEX_RPAREN);

            node_add_child(while_node, parse_block());

            peek_expect_advance(LEX_RBRACE);

            node_add_child(block_node, while_node);
        } else if (token.type == LEX_BREAK) {
            lexer_advance();

            node_t* break_node = node_create_leaf(BREAK_STATEMENT, token);

            node_add_child(block_node, break_node);

            peek_expect_advance(LEX_SEMICOLON);
        } else if (token.type == LEX_CONTINUE) {
            lexer_advance();

            node_t* continue_node = node_create_leaf(CONTINUE_STATEMENT, token);

            node_add_child(block_node, continue_node);

            peek_expect_advance(LEX_SEMICOLON);
        } else {
            fail_token(token);
        }
    }

    return block_node;
}

// FOLLOW: RPAREN
// TODO: semicolon in follow set
static node_t* parse_declaration_list() {
    node_t* decl_list = node_create(DECLARATION_LIST);
    token_t token = lexer_peek();
    if (token.type == LEX_RPAREN) {
        return decl_list;
    }
    node_t* decl = parse_declaration(NULL);
    node_add_child(decl_list, decl);

    // decl [, decl]*
    for (;;) {
        token = lexer_peek();

        if (token.type == LEX_RPAREN) {
            return decl_list;
        }

        peek_expect_advance(LEX_COMMA);
        decl = parse_declaration(NULL);
        node_add_child(decl_list, decl);
    }
}

static node_t* parse_type() {

    token_t token = lexer_peek();

    if (token.type == LEX_LPAREN) {
        return parse_function_type();
    }

    token = lexer_peek();

    if (token.type == LEX_OPERATOR) {
        // pointer type
        char * operator_str = lexer_substring(token.begin_offset, token.end_offset);
        operator_t op = parse_operator_str(operator_str, false);
        free(operator_str);

        if (op != UNARY_STAR) {
            fail_token(token);
        }

        lexer_advance();

        node_t* inner_type = parse_type();

        node_t* ptr_type = node_create(TYPE);
        ptr_type->data.type_class = TC_POINTER;
        node_add_child(ptr_type, inner_type);
        return ptr_type;
    } else if (token.type == LEX_STRUCT) {
        lexer_advance();

        node_t* type_node = node_create(TYPE);
        type_node->data.type_class = TC_STRUCT;

        node_t* struct_def = parse_struct_body();
        node_add_child(type_node, struct_def);

        peek_expect_advance(LEX_RBRACE);
        return type_node;
    }

    if (token.type != LEX_IDENTIFIER) {
        fail_token_expected(token, LEX_IDENTIFIER);
    }

    lexer_advance();

    node_t* identifier_node = node_create_leaf(IDENTIFIER, token);
    identifier_node->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);

    node_t* type_node = node_create(TYPE);
    type_node->data.type_class = TC_UNKNOWN;

    node_add_child(type_node, identifier_node);

    // TODO: Array type.
    if (lexer_peek().type == LEX_LBRACKET) {
        lexer_advance();

        node_t* array_type_node = node_create(TYPE);
        array_type_node->data.type_class = TC_ARRAY;

        node_add_child(array_type_node, type_node);
        node_t* dim_list = node_create(LIST);

        for (;;) {
            token_t literal_token = peek_expect_advance(LEX_INTEGER);
            char* literal_str = lexer_substring(literal_token.begin_offset, literal_token.end_offset);
            node_t* literal_node = node_create_leaf(INTEGER_LITERAL, literal_token);
            literal_node->data.int_literal_value = atol(literal_str);
            node_add_child(dim_list, literal_node);
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

        node_add_child(array_type_node, dim_list);
        return array_type_node;
    }
    return type_node;
}

// FOLLOW: LEX_RBRACE
static node_t* parse_struct_body() {
    peek_expect_advance(LEX_LBRACE);
    node_t* decls = node_create(DECLARATION_LIST);

    token_t token;

    for (;;) {
        token = lexer_peek();
        if (token.type == LEX_RBRACE) break;

        if (token.type == LEX_IDENTIFIER) {
            lexer_advance();
            node_t* identifier = node_create_leaf(IDENTIFIER, token);
            identifier->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);
            node_t* decl = parse_declaration(identifier);
            peek_expect_advance(LEX_SEMICOLON);
            node_add_child(decls, decl);

        } else {
            fail_token_expected(token, LEX_IDENTIFIER);
        }
    }
    return decls;
}

static node_t* parse_function_type() {
    // (DECLARATION_LIST) -> type
    peek_expect_advance(LEX_LPAREN);

    node_t* declaration_list = parse_declaration_list();

    peek_expect_advance(LEX_RPAREN);
    peek_expect_advance(LEX_ARROW);

    node_t* return_type = parse_type();

    node_t* function_type = node_create(TYPE);
    function_type->data.type_class = TC_FUNCTION;

    node_add_child(function_type, declaration_list);
    node_add_child(function_type, return_type);

    return function_type;
}

static bool has_precedence(operator_t operator_1, operator_t operator_2) {
    return OPERATOR_PRECEDENCE[operator_1] < OPERATOR_PRECEDENCE[operator_2];
}

// Find the correct place in `rhs` to insert `lhs`, preserving operator precedence.
static node_t* merge_subtrees(token_t operator_token, node_t* lhs, node_t* rhs) {
    char* lhs_operator_str = lexer_substring(operator_token.begin_offset, operator_token.end_offset);
    operator_t lhs_operator = parse_operator_str(lhs_operator_str, true);
    free(lhs_operator_str);

    if (rhs->type != OPERATOR || da_size(rhs->children) != 2 || has_precedence(rhs->data.operator, lhs_operator)) {
        node_t* operator_node;
        if (lhs_operator == BINARY_DOT) {
            if (rhs->type == DOT_ACCESS) {
                rhs->children[0] = merge_subtrees(operator_token, lhs, rhs->children[0]);
                return rhs;
            }
            //rhs->children[0] = merge_subtrees(operator_token, lhs, rhs->children[0]);
            operator_node = node_create(DOT_ACCESS);
        } else {
            operator_node = node_create(OPERATOR);
            operator_node->data.operator = lhs_operator;
            operator_node->pos = operator_token;
        }
        node_add_child(operator_node, lhs);
        node_add_child(operator_node, rhs);
        return operator_node;
    }


    rhs->children[0] = merge_subtrees(operator_token, lhs, rhs->children[0]);
    rhs->children[0]->parent = rhs;
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
        } else if (strcmp(operator_str, "%") == 0) {
            return BINARY_MOD;
        } else if (strcmp(operator_str, ">") == 0) {
            return BINARY_GT;
        } else if (strcmp(operator_str, "<") == 0) {
            return BINARY_LT;
        } else if (strcmp(operator_str, "+=") == 0) {
            return BINARY_ASS_ADD;
        } else if (strcmp(operator_str, "-=") == 0) {
            return BINARY_ASS_SUB;
        } else if (strcmp(operator_str, "*=") == 0) {
            return BINARY_ASS_MUL;
        } else if (strcmp(operator_str, "/=") == 0) {
            return BINARY_ASS_DIV;
        } else if (strcmp(operator_str, "%=") == 0) {
            return BINARY_ASS_MOD;
        } else if (strcmp(operator_str, "==") == 0) {
            return BINARY_EQ;
        } else if (strcmp(operator_str, "<=") == 0) {
            return BINARY_LEQ;
        } else if (strcmp(operator_str, ">=") == 0) {
            return BINARY_GEQ;
        } else if (strcmp(operator_str, "!=") == 0) {
            return BINARY_NEQ;
        } else if (strcmp(operator_str, "::") == 0) {
            return BINARY_SCOPE_RES;
        } else if (strcmp(operator_str, ".") == 0) {
            return BINARY_DOT;
        } else if (strcmp(operator_str, "||") == 0) {
            return BINARY_OR;
        } else if (strcmp(operator_str, "&&") == 0) {
            return BINARY_AND;
        } else if (strcmp(operator_str, ".*") == 0) { // eeeeh
            return UNARY_DEREF;
        }
    } else {
        if (strcmp(operator_str, "-") == 0) {
            return UNARY_SUB;
        } else if (strcmp(operator_str, "!") == 0) {
            return UNARY_NEG;
        } else if (strcmp(operator_str, "*") == 0) {
            return UNARY_STAR;
        } else if (strcmp(operator_str, ".*") == 0) {
            return UNARY_DEREF;
        }
    }
    // TODO: proper
    fail_token(lexer_peek());
    assert(false);
    // fprintf(stderr, "Parse operator str unhandled: %s, %d\n", operator_str, binary);
    // exit(1);
}

static node_t* merge_unary_op(token_t operator_token, node_t* rhs) {
    char* operator_str = lexer_substring(operator_token.begin_offset, operator_token.end_offset);
    operator_t operator = parse_operator_str(operator_str, false);
    free(operator_str);

    if (rhs->type != OPERATOR || da_size(rhs->children) != 2 || has_precedence(rhs->data.operator, operator)) {
        node_t* operator_node = node_create(OPERATOR);
        operator_node->data.operator = operator;

        operator_node->pos = operator_token; // store keyword token

        node_add_child(operator_node, rhs);

        return operator_node;
    }

    rhs->children[0] = merge_unary_op(operator_token, rhs->children[0]);
    return rhs;
}

inline static bool token_is_expression_end(token_t token) {
    return token.type == LEX_SEMICOLON
        || token.type == LEX_COMMA
        || token.type == LEX_RPAREN
        || token.type == LEX_RBRACKET;
}

static node_t* expression_continuation(token_t operator_token, node_t* lhs) {
    char *operator_str = lexer_substring(operator_token.begin_offset, operator_token.end_offset);
    operator_t operator = parse_operator_str(operator_str, true);
    free(operator_str);

    // TODO: more post operators
    if (operator == UNARY_DEREF) {
        node_t* new = node_create(OPERATOR);
        new->data.operator = operator;
        new->pos = operator_token; // store keyword token
        node_add_child(new, lhs);

        token_t nxt = lexer_peek();
        if (token_is_expression_end(nxt)) {
            return new;
        }

        if (nxt.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(nxt, new);
        } else {
            fail_token_expected(nxt, LEX_OPERATOR);
        }
    }

    node_t* rhs = parse_expression();

    return merge_subtrees(operator_token, lhs, rhs);
}

static bool token_is_literal(token_t token) {
    return 
        token.type == LEX_INTEGER 
     || token.type == LEX_REAL 
     || token.type == LEX_STRING 
     || token.type == LEX_TRUE 
     || token.type == LEX_FALSE
     || token.type == LEX_CHAR;
}

static char parse_char_escaped(const char *s) {
    if (s[0] == '\\') {
        switch (s[1]) {
            case 'n':  return '\n';
            case 't':  return '\t';
            case 'r':  return '\r';
            case '\\': return '\\';
            case '\'': return '\'';
            case '\"': return '\"';
            case '0':  return '\0';
            default:   return s[1]; // unknown escape
        }
    }
    return s[0];
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
    } else if (token_is_literal(token)) {
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
        } else if (token.type == LEX_CHAR) {
            node->type = CHAR_LITERAL;
            char* substr = lexer_substring(token.begin_offset + 1, token.end_offset - 1);
            node->data.char_literal_value = parse_char_escaped(substr);
            free(substr);
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
        node_add_child(ret, expr);

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
    } else if (token.type == LEX_ALLOC) {
        node_t* alloc_node = parse_alloc();
        token = lexer_peek();
        if (token.type == LEX_OPERATOR) {
            lexer_advance();
            return expression_continuation(token, alloc_node);
        } else if (token_is_expression_end(token)) {
            return alloc_node;
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
    node_add_child(ret, identifier_node);

    node_t* list_node = node_create(LIST);

    token = lexer_peek();
    while (token.type != LEX_RPAREN) {
        node_add_child(list_node, parse_expression());
        token = lexer_peek();
        if (token.type == LEX_RPAREN) break;
        token = peek_expect_advance(LEX_COMMA);
    }
    lexer_advance();

    node_add_child(ret, list_node);
    return ret;
}

static node_t* parse_cast() {
    // typename, 
    node_t* typename_node = parse_type();
    peek_expect_advance(LEX_COMMA);
    node_t* expr_node = parse_expression();
    node_t* ret = node_create(CAST_EXPRESSION);
    node_add_child(ret, typename_node);
    node_add_child(ret, expr_node);
    return ret;
}

// lvalue = expression
static node_t* parse_assignment(node_t* lhs_node) {
    peek_expect_advance(LEX_EQUAL);
    node_t* ret = node_create(ASSIGNMENT_STATEMENT);
    node_add_child(ret, lhs_node);

    token_t token = lexer_peek();

    if (token.type == LEX_LBRACE) {
        // TODO: reassign function??????
        fail_token(token);
    }

    node_add_child(ret, parse_expression());
    return ret;
}

// For when there is <something> <operator> <something>
// as a block statement
static node_t* parse_block_operation(node_t* lhs_node) {
    token_t operator_token = peek_expect_advance(LEX_OPERATOR);

    char* operator_str = lexer_substring(operator_token.begin_offset, operator_token.end_offset);
    operator_t op = parse_operator_str(operator_str, true);
    free(operator_str);

    if (op == BINARY_SCOPE_RES) {
        return parse_scope_resolution(lhs_node);
    } else if (op == BINARY_DOT) {
        return parse_dot_access(lhs_node);
    } else if (op == UNARY_DEREF) {
        return parse_deref(lhs_node);
    }
    
    // For now: assume this is a += or similar

    node_t* assignment_node = node_create(ASSIGNMENT_STATEMENT);
    node_add_child(assignment_node, lhs_node);

    node_t* rhs_node = parse_expression();

    node_t* operator_node = node_create(OPERATOR);

    operator_node->pos = operator_token;

    node_add_child(operator_node, node_deep_copy(lhs_node));
    node_add_child(operator_node, rhs_node);

    switch(op) {
        case BINARY_ASS_ADD:
            operator_node->data.operator = BINARY_ADD;
            break;
        case BINARY_ASS_SUB:
            operator_node->data.operator = BINARY_SUB;
            break;
        case BINARY_ASS_MUL:
            operator_node->data.operator = BINARY_MUL;
            break;
        case BINARY_ASS_DIV:
            operator_node->data.operator = BINARY_DIV;
            break;
        case BINARY_ASS_MOD:
            operator_node->data.operator = BINARY_MOD;
            break;

        default:
            fail_token(operator_token);
    }

    node_add_child(assignment_node, operator_node);
    return assignment_node;
}

static node_t* parse_scope_resolution(node_t* lhs_node) {
    token_t token = lexer_peek();
    if (token.type != LEX_IDENTIFIER) 
        fail_token_expected(token, LEX_IDENTIFIER);

    node_t* identifier = node_create_leaf(IDENTIFIER, token);
    identifier->data.identifier_str = lexer_substring(token.begin_offset, token.end_offset);

    lexer_advance();

    node_t* merged = node_create(SCOPE_RESOLUTION);
    node_add_child(merged, lhs_node);
    node_add_child(merged, identifier);

    token = lexer_peek();

    switch (token.type) {
        case LEX_LPAREN:
            return parse_function_call(merged);
        case LEX_OPERATOR:
            return parse_block_operation(merged);
        default:
            fail_token(token);
    }

    assert(false); // unreachable
}

static node_t* parse_dot_access(node_t* lhs_node) {
    token_t token = lexer_peek();

    if (token.type != LEX_IDENTIFIER && token.type != LEX_OPERATOR) {
        fail_token(token);
    }
    
    char* image = lexer_substring(token.begin_offset, token.end_offset);
    if (token.type == LEX_OPERATOR) {
        if (parse_operator_str(image, true) != BINARY_MUL) {
            fail_token(token);
        }
        // from here pretend it is an identifier
    }

    node_t* identifier = node_create_leaf(IDENTIFIER, token);
    identifier->data.identifier_str = image;
    lexer_advance();

    node_t* merged = node_create(DOT_ACCESS);
    node_add_child(merged, lhs_node);
    node_add_child(merged, identifier);

    token = lexer_peek();

    switch (token.type) {
        case LEX_OPERATOR:
            return parse_block_operation(merged);
        case LEX_SEMICOLON:
            return merged;
        case LEX_EQUAL:
            return parse_assignment(merged);
        default:
            fail_token(token);
    }

    assert(false); // unreachable
}

static node_t* parse_deref(node_t* lhs_node) {
    node_t* deref_node = node_create(OPERATOR);
    deref_node->data.operator = UNARY_DEREF;
    node_add_child(deref_node, lhs_node);

    token_t token = lexer_peek();

    switch (token.type) {
        case LEX_OPERATOR:
            return parse_block_operation(deref_node);
        case LEX_EQUAL:
            return parse_assignment(deref_node);
        case LEX_SEMICOLON:
        case LEX_RPAREN:
            return deref_node;
        default:
            fail_token(token);
    }
    assert(false); // unreachable
}

static node_t* parse_array_indexing(node_t* identifier_node) {
    peek_expect_advance(LEX_LBRACKET);
    node_t* indexing = node_create(ARRAY_INDEXING);
    node_add_child(indexing, identifier_node);

    node_t* index_list = node_create(LIST);

    for (;;) {
        node_t* expr = parse_expression();
        node_add_child(index_list, expr);
        token_t token = lexer_peek();

        if (token.type == LEX_COMMA) {
            lexer_advance();
            continue;
        } else if (token.type == LEX_RBRACKET) 
            break;
        fail_token(token);
    }
    peek_expect_advance(LEX_RBRACKET);
    node_add_child(indexing, index_list);
    return indexing;
}

static node_t* parse_alloc() {
    peek_expect_advance(LEX_ALLOC);
    peek_expect_advance(LEX_LPAREN);
    node_t* type_node = parse_type();
    token_t token = lexer_peek();
    node_t* alloc_node = node_create(ALLOC_EXPRESSION);
    node_add_child(alloc_node, type_node);
    if (token.type == LEX_COMMA) {
        lexer_advance();
        node_t* count = parse_expression();
        node_add_child(alloc_node, count);
    }
    peek_expect_advance(LEX_RPAREN);
    return alloc_node;
}

static token_t peek_expect_advance(token_type_t expected) {
    token_t token = lexer_peek();
    if (token.type != expected)
        fail_token_expected(token, expected);
    lexer_advance();
    return token;
}
