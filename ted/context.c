#include "context.h"
#include "ted_buffer.h"

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

    // command buf
    ctx->bufhead = calloc(1, sizeof(bufentry_t));
    tb_fill_from_string(&ctx->bufhead->buf, "", 0);
    ctx->buftail = ctx->bufhead;

    ctx->cmd_buf = ctx->bufhead;
    ctx->cur_buf = ctx_buf_push(ctx);
}

static bufentry_t* ctx_buf_push(context_t *ctx) {
    ctx->buftail->nxt = calloc(1, sizeof(bufentry_t));
    tb_fill_from_string(&ctx->buftail->nxt->buf, "", 0);
    ctx->buftail = ctx->buftail->nxt;
    return ctx->buftail;
}
