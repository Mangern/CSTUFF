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

// state
typedef struct context_t {
    int tabsize;
    struct ted_buffer_t main_buffer;
    struct ted_buffer_t cmd_buffer;
    editor_mode_t editor_mode;
    bool should_draw;
    bool should_resize;
    bool should_quit;
    struct winsize win_size;
} context_t;

#endif // CONTEXT_H
