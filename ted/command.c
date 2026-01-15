#include "command.h"

#include <string.h>

void parse_execute_command(context_t *ctx, char* cmd, int len) {
    if (strncmp(cmd, "q", len) == 0) {
        ctx->should_quit = true;
        return;
    }

    // TODO: error
}
