#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "da.h"
#include "json_rpc.h"
#include "json_types.h"
#include "log.h"

#define LINEBUF_SIZE 1024

static size_t line_len;
static char linebuf[LINEBUF_SIZE];

// header
static size_t content_length;
static char *content_type;

// content
static char *msg_content;

static void next_line();
static bool header_parse();

struct json_obj_t *next_request() {
    // parse header
    content_length = 0;
    for (;;) {
        next_line();

        if (header_parse()) break;
    }

    da_resize(msg_content, content_length);

    char *ptr = msg_content;
    size_t sum = 0;
    for (; sum < content_length; ) {
        ssize_t n = read(STDIN_FILENO, ptr, LINEBUF_SIZE - 1);
        sum += n;
        ptr += n;
    }
    // fwrite(msg_content, 1, content_length, LOG_FILE);
    // LOG("\n\n");


    json_init(msg_content);
    return json_parse();
}

void write_response(int64_t id, json_any_t result) {
    json_any_t msg = {0};
    json_obj_t msg_obj = {0};
    msg.kind = JSON_OBJ;
    msg.obj = &msg_obj;

    json_obj_put(&msg_obj, "id", JSON_ANY_NUM(id));
    json_obj_put(&msg_obj, "result", result);

    char *out = 0;
    json_dumps(&out, &msg);

    fprintf(stdout, "Content-Length: %zu\r\n\r\n", da_size(out));
    fprintf(stdout, "%s", out);
    fprintf(stderr, "%s", out);
    fflush(stdout);
    fflush(stderr);
}

void next_line() {
    for (line_len = 0; line_len < LINEBUF_SIZE; ++line_len) {
        char c = getchar();

        if (c == '\n') {
            linebuf[line_len++] = c;
            linebuf[line_len] = 0;
            break;
        }

        linebuf[line_len] = c;
    }

}

bool header_parse() {
    char *ptr = strtok(linebuf, ":\r\n");

    if (ptr == NULL) {
        return true;
    }

    if (strcmp(ptr, "Content-Length") == 0) {
        ptr = strtok(NULL, ":\r\n");
        while (*ptr == ' ')++ptr;
        content_length = atol(ptr);
        return false;
    }
    if (strcmp(ptr, "Content-Type") == 0) {
        ptr = strtok(NULL, ":\r\n");
        while (*ptr == ' ')++ptr;
        content_type = strdup(ptr);
        return false;
    }

    log_writes("Unknown header!!\n");
    log_writes(ptr);
    exit(1);
    return false;
}

