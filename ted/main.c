#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "gap_buffer.h"

typedef struct gap_buffer_t gap_buffer_t;

#define MAX_LINES (1<<17)

static struct termios orig_termios;
static bool opt_debug = 0;

static int KEY_LEFT      = 0x445b1b;
static int KEY_UP        = 0x415b1b;
static int KEY_DOWN      = 0x425b1b;
static int KEY_RIGHT     = 0x435b1b;
static int KEY_BACKSPACE = 0x7f;
static int KEY_ESC       = 0x1b;
static int KEY_TAB       = 0x9;

static int tabsize = 4;

// state

long num_lines = 0;
// buffer does not contain trailing newline
gap_buffer_t line_buffers[MAX_LINES];

int cur_line;
int cur_character;

// Save original terminal settings
void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
}

// Put terminal in raw mode (no buffering, no echo)
void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(0, &orig_termios);
    atexit(reset_terminal_mode);

    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

void debug_keyboard() {
    printf("Running in debug mode\n");
    fflush(stdout);

    while (1) {
        if (kbhit()) {
            int c = 0;
            int n = read(0, &c, 4);
            if (n > 0) {
                printf("%x\n", c);
                fflush(stdout);
                // EOF
                if (c == 4) break;
            } else if (n == 0) {
                break;
            }
        }
        // Do other work here
        usleep(1000);
    }
}

void options(int argc, char **argv) {
    for (;;) {
        switch(getopt(argc, argv, "d")) {
            case 'd':
                opt_debug = 1;
                break;
            default:
                return;
        }
    }
}

bool buffer_char(int c) {
    return isascii(c) && isprint(c);
}

void init_line_buffers(char* content, size_t length) {
    gap_buffer_init(&line_buffers[0]);
    for (size_t i = 0; i < length; ++i) {
        if (content[i] == '\n') {
            gap_buffer_init(&line_buffers[++num_lines]);
        } else {
            gap_buffer_gap_insert(&line_buffers[num_lines], content[i]);
        }
    }
    ++num_lines;
}

void insert_line_after(int line) {
    for (int i = num_lines - 1; i > line; --i) {
        line_buffers[i+1] = line_buffers[i];
    }
    gap_buffer_init(&line_buffers[line+1]);
    ++num_lines;
}

void delete_line(int line) {
    assert(0 <= line && line < num_lines);

    gap_buffer_deinit(&line_buffers[line]);
    for (int i = line; i + 1 < num_lines; ++i) {
        line_buffers[i] = line_buffers[i+1];
    }
    --num_lines;
}

void constrain_line_char() {
    assert(num_lines > 0);
    if (cur_line < 0) cur_line = 0;
    if (cur_line >= num_lines) cur_line = (int)num_lines - 1;
    if (cur_character < 0) {
        cur_character = 0;
    }
    size_t count = gap_buffer_count(&line_buffers[cur_line]);
    if (cur_character > count)cur_character = count;
}

int main(int argc, char **argv) {
    options(argc, argv);

    set_conio_terminal_mode();

    if (opt_debug) {
        debug_keyboard();
        return 0;
    }

    if (optind >= argc) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* file_name = argv[optind];
    ++optind; // hmm,

    FILE * read_file = fopen(file_name, "r");

    if (!read_file) {
        fprintf(stderr, "ERROR: Failed to read file %s\n", file_name);
        exit(-1);
    }

    {
        const size_t CHUNK = 1024;
        size_t cap = CHUNK;
        size_t size = 0;
        char* str = malloc(cap);

        for (;;) {
            long nread = fread(str+size, 1, cap - size, read_file);
            if (nread == 0) break;
            size += nread;

            if (size == cap) {
                cap = cap * 3 / 2;
                str = realloc(str, cap);
            }
        }
        init_line_buffers(str, size);
        fclose(read_file);
        free(str);
    }

    char* print_buf = malloc(GAP_BUFFER_SIZE);
    cur_line = (int)num_lines - 1;
    cur_character = 0;

    while (1) {
        constrain_line_char();
        // move home, erase until end
        printf("\x1B[H\x1B[0J");
        for (int i = 0; i < num_lines; ++i) {
            size_t count = gap_buffer_count(&line_buffers[i]);
            gap_buffer_str(&line_buffers[i], print_buf);
            printf("%.*s\n", (int)count, print_buf);
        }
        // move to current location
        printf("\x1B[%d;%dH", cur_line+1, cur_character+1);
        fflush(stdout);
        if (kbhit()) {
            int c = 0;
            int n = read(0, &c, 4);
            if (n > 0) {
                if (buffer_char(c)) {
                    gap_buffer_gap_at(&line_buffers[cur_line], cur_character);
                    gap_buffer_gap_insert(&line_buffers[cur_line], c);
                    cur_character += 1;
                    //buffer[buffer_count++] = c;
                } else if (c == '\n') {
                    // TODO: chop current line and put content in next line
                    insert_line_after(cur_line);
                    ++cur_line;
                    cur_character = 0;
                } else if (c == KEY_BACKSPACE) {
                    if (cur_character > 0) {
                        gap_buffer_gap_at(&line_buffers[cur_line], cur_character);
                        gap_buffer_gap_delete(&line_buffers[cur_line]);
                        --cur_character;
                    } else if (cur_line > 0) {
                        // TODO: slap cur line content on prev line
                        delete_line(cur_line);
                        --cur_line;
                        cur_character = gap_buffer_count(&line_buffers[cur_line]);
                    }
                } else if (c == KEY_TAB) {
                    gap_buffer_gap_at(&line_buffers[cur_line], cur_character);
                    for (int i = 0; i < tabsize; ++i) {
                       gap_buffer_gap_insert(&line_buffers[cur_line], ' ');
                       cur_character += 1;
                    }
                } else if (c == KEY_UP) {
                    --cur_line;
                } else if (c == KEY_DOWN) {
                    ++cur_line;
                } else if (c == KEY_LEFT) {
                    --cur_character;
                } else if (c == KEY_RIGHT) {
                    ++cur_character;
                }
                // EOF
                if (c == 4) break;
            } else if (n == 0) {
                break;
            }
        }
        // Do other work here
        usleep(1000);
    }

    FILE * write_file = fopen(file_name, "w");
    if (!write_file) {
        fprintf(stderr, "Failed to write to %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_lines; ++i) {
        size_t count = gap_buffer_count(&line_buffers[i]);
        gap_buffer_str(&line_buffers[i], print_buf);
        fprintf(write_file, "%.*s", (int)count, print_buf);
        if (i + 1 < num_lines) {
            fprintf(write_file, "\n");
        }

        // deinit
        gap_buffer_deinit(&line_buffers[i]);
    }

    if (fclose(write_file)) {
        fprintf(stderr, "Failed to close file %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    return 0;
}
