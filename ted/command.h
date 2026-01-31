#ifndef COMMAND_H
#define COMMAND_H

#include "context.h"

#include <stdbool.h>

struct cmd_result_t {
    bool err;
    char *emsg;
};

struct cmd_result_t parse_execute_command(context_t *ctx, char* cmd, int len);

struct cmd_result_t cmd_write_file(context_t *ctx);

// If cur_buf has a filename or it has content, create a new buffer. Otherwise, replace cur_buf with the opened file.
// The buffer takes ownership of file_name!
struct cmd_result_t cmd_edit_file(context_t *ctx, char* file_name);

struct cmd_result_t cmd_expand_tree(context_t *ctx);

struct cmd_result_t cmd_collapse_tree(context_t *ctx);

#endif // COMMAND_H
