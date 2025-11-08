#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "json.h"
#include "da.h"

#define LINEBUF_SIZE 1024

size_t line_len;
char linebuf[LINEBUF_SIZE];

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

void linecheck() {
    if (line_len >= LINEBUF_SIZE) {
        log_writes("Unexpectedly long line!\n");
        linebuf[LINEBUF_SIZE - 1] = 0;
        log_writes(linebuf);
        exit(1);
    }
}

size_t content_length;
char *content_type;

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
    return false;
}


int main() {
    setvbuf(stdin, NULL, _IONBF, 0);
    log_init();
    char *msg_content = 0;

    for (;;) {
        // parse header
        content_length = 0;
        for (;;) {
            next_line();

            if (header_parse()) break;
        }

        LOG("Content length: %zu", content_length);

        da_resize(msg_content, content_length);

        char *ptr = msg_content;
        size_t sum = 0;
        for (; sum < content_length; ) {
            ssize_t n = read(STDIN_FILENO, ptr, LINEBUF_SIZE - 1);
            sum += n;
            ptr += n;
        }
        LOG("Successfully read %zu bytes of message", content_length);
        fwrite(msg_content, 1, content_length, LOG_FILE);
        LOG("\n\n");


        json_init(msg_content);

        struct json_obj_t *obj = json_parse();

        // parse the json

        struct json_any_t method_val = json_obj_get(obj, "method");

        if (method_val.kind == JSON_STR) {
            LOG("I demonstrate my ability to parse json by showing that 'method' = %s", method_val.str);
        } else {
            LOG("Expected method to be str, but was %d", method_val.kind);
            exit(1);
        }
    }

    fclose(LOG_FILE);
    return 1;
}
