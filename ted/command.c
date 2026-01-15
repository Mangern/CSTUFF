#include "command.h"
#include "gap_buffer.h"
#include "ted_buffer.h"

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum token_type_t {
    TOK_ID,
    TOK_END,
    TOK_ERR
} token_type_t;

typedef struct token_t {
    token_type_t type;
    int beg;
    int end;
} token_t;

typedef struct lex_t {
    char *content;
    int ptr;
    int len;
    token_t cur_tok;
} lex_t;

static token_t peek(lex_t *lex);
static void advance(lex_t *lex);
static void skip_ws(lex_t *lex);
static int match_id(lex_t *lex);
static char* lex_strdup(lex_t *lex);

static void cmd_write_file(context_t *ctx);

void parse_execute_command(context_t *ctx, char* cmd, int len) {
    lex_t lex = {
        .content = cmd,
        .ptr = 0,
        .len = len,
        .cur_tok = {
            .beg = -1,
        }
    };

    token_t token = peek(&lex);

    // TODO
    if (token.type == TOK_ERR) return;

    if (token.type == TOK_END) return;

    assert((token.type == TOK_ID) && "Not implemented");

    if (strncmp(&lex.content[token.beg], "q", token.end - token.beg) == 0) {
        ctx->should_quit = true;
        return;
    }

    if (strncmp(&lex.content[token.beg], "w", token.end - token.beg) == 0) {
        advance(&lex);
        token = peek(&lex);

        // TODO
        switch (token.type) {
            case TOK_ERR:
                return;
            case TOK_END:
                if (ctx->cur_buf->file_name == 0) {
                    // TODO: Error: no file name provided
                    return;
                }
                break;
            case TOK_ID:
                {
                    char *new_filename = lex_strdup(&lex);
                    if (ctx->cur_buf->file_name != 0) {
                        free(ctx->cur_buf->file_name);
                    }
                    ctx->cur_buf->file_name = new_filename;
                }
                break;
            default:
                assert(false && "Not implemented");
        }
        cmd_write_file(ctx);
        return;
    }

    // TODO: error: unknown command
}

static token_t peek(lex_t *lex) {
    if (lex->ptr == lex->cur_tok.beg) {
        return lex->cur_tok;
    }
    skip_ws(lex);

    if (lex->ptr == lex->len) {
        lex->cur_tok = (token_t){
            .type = TOK_END,
            .beg = lex->ptr,
            .end = lex->ptr
        };
        return lex->cur_tok;
    }

    int match_len;

    if ((match_len = match_id(lex))) {
        lex->cur_tok = (token_t){
            .type = TOK_ID,
            .beg = lex->ptr,
            .end = lex->ptr + match_len
        };
        return lex->cur_tok;
    }

    lex->cur_tok = (token_t){
        .type = TOK_ERR,
        .beg = lex->ptr,
        .end = lex->ptr + 1
    };
    return lex->cur_tok;
}

static void advance(lex_t *lex) {
    lex->ptr = lex->cur_tok.end;
}

static void skip_ws(lex_t *lex) {
    while (lex->ptr < lex->len && isspace(lex->content[lex->ptr])) {
        ++lex->ptr;
    }
}

static int match_id(lex_t *lex) {
    int ptr = lex->ptr;
    while (ptr < lex->len && !isspace(lex->content[ptr])) {
        ++ptr;
    }
    return ptr - lex->ptr;
}

static char* lex_strdup(lex_t *lex) {
    char *str = &lex->content[lex->cur_tok.beg];
    return strndup(str, lex->cur_tok.end - lex->cur_tok.beg);
}

// ==== Commands ====

static void cmd_write_file(context_t *ctx) {
    char print_buf[GAP_BUFFER_SIZE];

    assert(ctx->cur_buf->file_name != 0);

    FILE * write_file = fopen(ctx->cur_buf->file_name, "w");
    if (!write_file) {
        // TODO: error
        // fprintf(stderr, "Failed to write to %s\n", file_name);
        // exit(EXIT_FAILURE);
        return;
    }

    struct ted_buffer_t *buf = &ctx->cur_buf->buf;
    
    for (int i = 0; i < buf->num_lines; ++i) {
        size_t count = gap_buffer_count(buf->line_bufs[i]);
        gap_buffer_str(buf->line_bufs[i], print_buf);
        fprintf(write_file, "%.*s\n", (int)count, print_buf);
        // deinit
    }
    
    if (fclose(write_file)) {
        //fprintf(stderr, "Failed to close file %s\n", file_name);
        //exit(EXIT_FAILURE);
    }

    // TODO: log message
}
