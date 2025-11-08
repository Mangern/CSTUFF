#include "log.h"
#include <stdio.h>
#include <stdlib.h>

FILE *LOG_FILE;

const char *LOG_PATH = "/home/magnus/repos/cstuff/lang-lsp/log.txt";

void log_init() {
    LOG_FILE = fopen(LOG_PATH, "w");

    if (!LOG_FILE) {
        fprintf(stderr, "Failed to open log file!\n");
        exit(1);
    }
}

void log_writec(char c) {
    fprintf(LOG_FILE, "%c", c);
}

void log_writes(char *str) {
    fprintf(LOG_FILE, "%s", str);
}

