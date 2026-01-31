#include "command.h"
#include "context.h"
#include "gap_buffer.h"
#include "ted_buffer.h"

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
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

typedef struct cmd_result_t cmd_result_t;

#define EMSG_SIZE 8192
char emsg_buf[EMSG_SIZE];

#define OK ((cmd_result_t){.err=false})
#define ERR(msg) ((cmd_result_t){.err=true,.emsg=(msg)})
#define ERRF(fmt, ...) ((cmd_result_t){.err=snprintf(emsg_buf, EMSG_SIZE, (fmt) __VA_OPT__(,) __VA_ARGS__),.emsg=emsg_buf})


static token_t peek(lex_t *lex);
static void advance(lex_t *lex);
static void skip_ws(lex_t *lex);
static int match_id(lex_t *lex);
static char* lex_strdup(lex_t *lex);

struct cmd_result_t parse_execute_command(context_t *ctx, char* cmd, int len) {
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
    if (token.type == TOK_ERR) return ERR("Unexpected token");

    // 
    if (token.type == TOK_END) return OK;

    assert((token.type == TOK_ID) && "Not implemented");

    if (strncmp(&lex.content[token.beg], "q", token.end - token.beg) == 0) {
        ctx->should_quit = true;
        return OK;
    }

    if (strncmp(&lex.content[token.beg], "w", token.end - token.beg) == 0) {
        advance(&lex);
        token = peek(&lex);

        // TODO
        switch (token.type) {
            case TOK_ERR:
                return ERR("Unexpected token");
            case TOK_END:
                if (ctx->cur_buf->file_name == 0) {
                    return ERR("No file name");
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
        return cmd_write_file(ctx);
    }
    
    if (strncmp(&lex.content[token.beg], "wq", token.end - token.beg) == 0) {
        if (ctx->cur_buf->file_name == 0) {
            return ERR("No file name");
        }
        cmd_result_t res = cmd_write_file(ctx);
        if (res.err) return res;
        ctx->should_quit = true;
        return OK;
    }

    if (strncmp(&lex.content[token.beg], "e", token.end - token.beg) == 0) {
        advance(&lex);
        token = peek(&lex);
        switch (token.type) {
            case TOK_ERR:
                return ERR("Unexpected token");
            case TOK_END:
                return ERR("Missing file name");
            case TOK_ID:
                {
                    char *filename = lex_strdup(&lex);
                    return cmd_edit_file(ctx, filename);
                }
                break;
            default:
                assert(false && "Not implemented");
        }
    }

    if (strncmp(&lex.content[token.beg], "log", token.end - token.beg) == 0) {
        ctx->cur_buf = ctx->log_buf;
        return OK;
    }

    return ERRF("Unknown command: %*s", token.end - token.beg, &lex.content[token.beg]);
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

cmd_result_t cmd_write_file(context_t *ctx) {
    char print_buf[GAP_BUFFER_SIZE];

    assert(ctx->cur_buf->file_name != 0);

    FILE * write_file = fopen(ctx->cur_buf->file_name, "w");
    if (!write_file) {
        // TODO: error
        // fprintf(stderr, "Failed to write to %s\n", file_name);
        // exit(EXIT_FAILURE);
        return ERRF("Cannot open %s for writing", ctx->cur_buf->file_name);
    }

    struct ted_buffer_t *buf = &ctx->cur_buf->buf;

    long num_write = 0;
    
    for (int i = 0; i < buf->num_lines; ++i) {
        size_t count = gap_buffer_count(buf->line_bufs[i]);
        gap_buffer_str(buf->line_bufs[i], print_buf);
        num_write += fprintf(write_file, "%.*s\n", (int)count, print_buf);
        // deinit
    }
    
    if (fclose(write_file)) {
        return ERRF("Failed to close %s", ctx->cur_buf->file_name);
    }

    ctx_logf(ctx, "\"%s\" %dL, %ldB written", ctx->cur_buf->file_name, buf->num_lines, num_write);
    return OK;
}

cmd_result_t cmd_edit_file(context_t *ctx, char* file_name) {
    // TODO: check if we have it open
    bool open_new = ctx->cur_buf->file_name != 0 
        || ctx->cur_buf->buf.num_lines > 1 
        || gap_buffer_count(ctx->cur_buf->buf.line_bufs[0]) > 0;

    if (open_new) {
        ctx_open_empty(ctx);
    }


    FILE * read_file = fopen(file_name, "r");
    
    if (!read_file) {
        if (errno == ENOENT) {
            // File did not exist.
            ctx->cur_buf->file_name = file_name;
            return OK;
        }
        return ERRF("ERROR: Failed to read file %s.", file_name);
    }

    // A bit ugly, just to remove 1 empty line in most cases
    while (ctx->cur_buf->buf.num_lines > 0) {
        tb_delete_line(&ctx->cur_buf->buf, ctx->cur_buf->buf.num_lines - 1);
    }
    
    const size_t CHUNK = 1024;
    size_t cap = CHUNK;
    size_t size = 0;
    char* str = malloc(cap);

    for (;;) {
        long nread = fread(str+size, 1, cap - size, read_file);
        if (nread == 0) break;
        size += nread;

        if (size == cap) {
            cap = cap * 3 / 2;
            str = realloc(str, cap);
        }
    }
    tb_fill_from_string(&ctx->cur_buf->buf, str, size);
    fclose(read_file);
    free(str);

    ctx->cur_buf->file_name = file_name;

    return OK;
}
