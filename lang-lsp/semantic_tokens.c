#include <stdlib.h>
#include <string.h>

#include "da.h"
#include "json.h"
#include "json_rpc.h"
#include "langc.h"
#include "log.h"
#include "semantic_tokens.h"
#include "lex.h"
#include "symbol.h"
#include "tree.h"
#include "type.h"

static void traverse_semantic_tokens(node_t *node, json_any_t **data);
static int64_t prev_line;
static int64_t prev_char;

char* TOKEN_TYPES[] = {
    "type",
    "struct",
    "parameter",
    "variable",
    "property",
    "function",
    "keyword",
    "comment",
    "string",
    "number",
    "operator"
};

json_obj_t* get_semantic_tokens_options() {
    json_obj_t* json_options = calloc(1, sizeof(json_obj_t));
    json_obj_put(json_options, "full", JSON_ANY_BOOL(true));

    // create semantictokenlegend
    json_obj_t* json_semantic_tokens_legend = calloc(1, sizeof(json_obj_t));

    json_arr_t* json_arr_modifiers = calloc(1, sizeof(json_arr_t));
    json_arr_t* json_arr_token_types = calloc(1, sizeof(json_arr_t));

    for (size_t i = 0; i < sizeof(TOKEN_TYPES) / sizeof(char*); ++i) {
        da_append(json_arr_token_types->elements, JSON_ANY_STR(TOKEN_TYPES[i]));
    }

    json_obj_put(json_semantic_tokens_legend, "tokenModifiers", JSON_ANY_ARR(json_arr_modifiers));
    json_obj_put(json_semantic_tokens_legend, "tokenTypes", JSON_ANY_ARR(json_arr_token_types));

    json_obj_put(json_options, "legend", JSON_ANY_OBJ(json_semantic_tokens_legend));

    return json_options;
}

/*
 * TODO: It is possible to be smart and only compute the necessary token (usually just like one)
 */
void handle_semantic_tokens(int64_t request_id, node_t *root) {
    json_arr_t token_data = {0};
    prev_line = 0;
    prev_char = 0;

    traverse_semantic_tokens(root, &token_data.elements);

    json_obj_t result = {0};

    json_obj_put(&result, "data", JSON_ANY_ARR(&token_data));

    write_response(request_id, JSON_ANY_OBJ(&result));

    char *str = 0;
    json_dumps(&str, JSON_ANY_OBJ(&result));

    LOG("%s", str);
}

void append_token(json_any_t **data, char *type_name, range_t range) {
    int64_t line       = range.start.line;
    int64_t start_char = range.start.character;
    int64_t len        = range.end.character - start_char; // len

    if (line == prev_line) {
        start_char = start_char - prev_char;
    }

    line = line - prev_line;

    LOG("At line %ld, transformed to %ld", (int64_t)range.start.line, line);

    int64_t type_idx;
    int64_t num_token_types = sizeof(TOKEN_TYPES) / sizeof(char*);
    for (type_idx = 0; type_idx < num_token_types; ++type_idx) {
        if (strcmp(TOKEN_TYPES[type_idx], type_name) == 0) {
            break;
        }
    }

    if (type_idx == num_token_types) {
        fprintf(stderr, "Invalid token type name %s supplied to append_token\n", type_name);
        exit(1);
    }

    da_append(*data, JSON_ANY_NUM(line));
    da_append(*data, JSON_ANY_NUM(start_char));
    da_append(*data, JSON_ANY_NUM(len));
    da_append(*data, JSON_ANY_NUM(type_idx));
    da_append(*data, JSON_ANY_NUM(0));

    prev_line = range.start.line;
    prev_char = range.start.character;
}

void traverse_semantic_tokens(node_t *node, json_any_t **data) {
    if (node->leaf) {
        if (node->type == STRING_LITERAL || node->type == CHAR_LITERAL) {
            range_t range = {
                .start = lexer_offset_location(node->pos.begin_offset),
                .end = lexer_offset_location(node->pos.end_offset)
            };

            append_token(data, "string", range);
        }

        if (node->type == INTEGER_LITERAL) {
            range_t range = {
                .start = lexer_offset_location(node->pos.begin_offset),
                .end = lexer_offset_location(node->pos.end_offset)
            };

            append_token(data, "number", range);
        }

        if (node->type == IDENTIFIER) {
            if (node->symbol != NULL && node->type_info != NULL) {
                type_info_t* type_info = node->type_info;
                range_t range = {
                    .start = lexer_offset_location(node->pos.begin_offset),
                    .end = lexer_offset_location(node->pos.end_offset)
                };

                if (type_info->type_class == TC_BASIC) {
                    append_token(data, "variable", range);
                } else if (type_info->type_class == TC_FUNCTION) {
                    append_token(data, "function", range);
                }
            } else if (node->symbol != NULL) {
                // most likely a builtin
            } else if (node->parent != NULL && node->parent->type == TYPE) {
                range_t range = {
                    .start = lexer_offset_location(node->pos.begin_offset),
                    .end = lexer_offset_location(node->pos.end_offset)
                };
                append_token(data, "type", range);
            }
        }

        if (node->type == BREAK_STATEMENT) {
            range_t range = {
                .start = lexer_offset_location(node->pos.begin_offset),
                .end = lexer_offset_location(node->pos.end_offset)
            };
            append_token(data, "keyword", range);
        }


        return;
    }

    // non-leaf handling (should be mostly keywords)
    if (node->type == RETURN_STATEMENT && node->pos.type == LEX_RETURN) {
        // pos.type must be return when it is not fake
        range_t range = {
            .start = lexer_offset_location(node->pos.begin_offset),
            .end = lexer_offset_location(node->pos.end_offset)
        };
        append_token(data, "keyword", range);
    }

    if (node->type == IF_STATEMENT && node->pos.type == LEX_IF) {
        range_t range = {
            .start = lexer_offset_location(node->pos.begin_offset),
            .end = lexer_offset_location(node->pos.end_offset)
        };
        append_token(data, "keyword", range);

        // expression
        traverse_semantic_tokens(node->children[0], data);

        // block
        traverse_semantic_tokens(node->children[1], data);

        if (da_size(node->children) == 3) {
            // else!!
            token_t else_pos = node->children[2]->pos;
            if (else_pos.type == LEX_ELSE) {
                range_t range = {
                    .start = lexer_offset_location(else_pos.begin_offset),
                    .end = lexer_offset_location(else_pos.end_offset)
                };
                append_token(data, "keyword", range);
            }
            traverse_semantic_tokens(node->children[2], data);
        }

        return;
    }

    if (node->type == WHILE_STATEMENT && node->pos.type == LEX_WHILE) {
        range_t range = {
            .start = lexer_offset_location(node->pos.begin_offset),
            .end = lexer_offset_location(node->pos.end_offset)
        };
        append_token(data, "keyword", range);
    }

    if (node->type == OPERATOR && node->pos.type == LEX_OPERATOR) {
        range_t range = {
            .start = lexer_offset_location(node->pos.begin_offset),
            .end = lexer_offset_location(node->pos.end_offset)
        };
        append_token(data, "operator", range);
    }

    for (size_t i = 0; i < da_size(node->children); ++i) {
        traverse_semantic_tokens(node->children[i], data);
    }
}
