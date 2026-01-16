#ifndef CONTEXT_H
#define CONTEXT_H

#include "ted_buffer.h"

#include <stdbool.h>
#include <sys/ioctl.h>

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND
} editor_mode_t;

extern const char* EDITOR_MODE_STR[];

// linked list of buffers
struct bufentry_t {
    struct bufentry_t*  nxt;
    struct bufentry_t*  prv;
    struct ted_buffer_t buf;
    char *file_name;
};

// state
typedef struct context_t {
    bool should_draw;
    bool should_resize;
    bool should_quit;
    int tabsize;
    editor_mode_t editor_mode;
    struct winsize win_size;
    struct bufentry_t *bufhead;
    struct bufentry_t *buftail;
    struct bufentry_t *cmd_buf;
    struct bufentry_t *log_buf;
    struct bufentry_t *cur_buf; // active buffer
} context_t;

void ctx_init(context_t *ctx);

// Create a new buffer and set cur_buf to that buffer.
void ctx_open_empty(context_t *ctx);

void ctx_log(context_t *ctx, char *message);
void ctx_logf(context_t *ctx, char *message, ...);

void ctx_deinit(context_t *ctx);

#endif // CONTEXT_H
