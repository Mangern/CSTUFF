#ifndef LOG_H
#define LOG_H

#include <stdio.h>

extern const char* LOG_PATH;
extern FILE *LOG_FILE;

#define LOG(fmt, ...) { fprintf(LOG_FILE, fmt "\n" __VA_OPT__(,) __VA_ARGS__); fflush(LOG_FILE); }
void log_init();
void log_writec(char c);
void log_writes(char *str);

#endif // LOG_H
