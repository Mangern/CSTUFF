#ifndef COMMAND_H
#define COMMAND_H

#include "context.h"

#include <stdbool.h>

struct cmd_result_t {
    bool err;
    char *emsg;
};

struct cmd_result_t parse_execute_command(context_t *ctx, char* cmd, int len);

#endif // COMMAND_H
