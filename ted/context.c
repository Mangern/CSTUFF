#include "context.h"
#include "ted_buffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct bufentry_t bufentry_t;

static bufentry_t* ctx_buf_push(context_t *ctx);

const char* EDITOR_MODE_STR[] = {
    "NORMAL",
    "INSERT",
    "CMD"
};

void ctx_init(context_t *ctx) {
    ctx->should_draw = 1;
    ctx->should_resize = 1;
    ctx->editor_mode = MODE_NORMAL;
    ctx->tabsize = 4;

    // Initialize with cmd buf, log buf and a new buffer for writing
    ctx->bufhead = calloc(1, sizeof(bufentry_t));
    tb_fill_from_string(&ctx->bufhead->buf, "", 0);
    ctx->buftail = ctx->bufhead;
    ctx->cmd_buf = ctx->bufhead;

    ctx->log_buf = ctx_buf_push(ctx);
    ctx->cur_buf = ctx_buf_push(ctx);
}

void ctx_log(context_t *ctx, char *message) {
    struct ted_buffer_t *buf = &ctx->log_buf->buf;
    tb_insert_line_after(buf, buf->num_lines - 1);
    buf->cur_line = buf->num_lines - 1;
    gap_buffer_gap_at(buf->line_bufs[buf->cur_line], 0);
    for (char *c = message; *c; ++c) {
        if (*c == '\n') {
            gap_buffer_gap_insert(buf->line_bufs[buf->cur_line], '\\');
            gap_buffer_gap_insert(buf->line_bufs[buf->cur_line], 'n');
            continue;
        }
        gap_buffer_gap_insert(buf->line_bufs[buf->cur_line], *c);
    }
}

void ctx_logf(context_t *ctx, char *message, ...) {
    char LOG_BUF[8192];

    va_list args;
    va_start(args, message);
    vsnprintf(LOG_BUF, 8192, message, args);
    va_end(args);

    ctx_log(ctx, LOG_BUF);
}

// ========== Internal ==========

static bufentry_t* ctx_buf_push(context_t *ctx) {
    ctx->buftail->nxt = calloc(1, sizeof(bufentry_t));
    tb_fill_from_string(&ctx->buftail->nxt->buf, "", 0);
    ctx->buftail = ctx->buftail->nxt;
    return ctx->buftail;
}
