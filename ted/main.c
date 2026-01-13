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
#include "ted_buffer.h"

typedef struct gap_buffer_t gap_buffer_t;
typedef struct ted_buffer_t ted_buffer_t;

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
// buffer does not contain trailing newline
ted_buffer_t main_buffer;
char* print_buf;

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

void draw() {
    // move home, erase until end
    printf("\x1B[H\x1B[0J");
    for (int i = 0; i < main_buffer.num_lines; ++i) {
        size_t count = gap_buffer_count(main_buffer.line_bufs[i]);
        gap_buffer_str(main_buffer.line_bufs[i], print_buf);
        printf("%.*s\n", (int)count, print_buf);
    }
    // move to current location
    printf("\x1B[%d;%dH", main_buffer.cur_line+1, main_buffer.cur_character+1);
    fflush(stdout);
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
        tb_fill_from_string(&main_buffer, str, size);
        fclose(read_file);
        free(str);
    }

    assert(main_buffer.num_lines > 0);

    print_buf = malloc(GAP_BUFFER_SIZE);
    main_buffer.cur_line = (int)main_buffer.num_lines - 1;
    main_buffer.cur_character = 0;

    draw();

    while (1) {
        tb_constrain_line_char(&main_buffer);
        if (kbhit()) {
            int c = 0;
            int n = read(0, &c, 4);
            if (n > 0) {
                if (buffer_char(c)) {
                    gap_buffer_gap_at(main_buffer.line_bufs[main_buffer.cur_line], main_buffer.cur_character);
                    gap_buffer_gap_insert(main_buffer.line_bufs[main_buffer.cur_line], c);
                    main_buffer.cur_character += 1;
                    //buffer[buffer_count++] = c;
                } else if (c == '\n') {
                    // TODO: chop current line and put content in next line
                    tb_insert_line_after(&main_buffer, main_buffer.cur_line);
                    ++main_buffer.cur_line;
                    main_buffer.cur_character = 0;
                } else if (c == KEY_BACKSPACE) {
                    if (main_buffer.cur_character > 0) {
                        gap_buffer_gap_at(main_buffer.line_bufs[main_buffer.cur_line], main_buffer.cur_character);
                        gap_buffer_gap_delete(main_buffer.line_bufs[main_buffer.cur_line]);
                        --main_buffer.cur_character;
                    } else if (main_buffer.cur_line > 0) {
                        // TODO: slap cur line content on prev line
                        tb_delete_line(&main_buffer, main_buffer.cur_line);
                        --main_buffer.cur_line;
                        main_buffer.cur_character = gap_buffer_count(main_buffer.line_bufs[main_buffer.cur_line]);
                    }
                } else if (c == KEY_TAB) {
                    gap_buffer_gap_at(main_buffer.line_bufs[main_buffer.cur_line], main_buffer.cur_character);
                    for (int i = 0; i < tabsize; ++i) {
                       gap_buffer_gap_insert(main_buffer.line_bufs[main_buffer.cur_line], ' ');
                       main_buffer.cur_character += 1;
                    }
                } else if (c == KEY_UP) {
                    --main_buffer.cur_line;
                } else if (c == KEY_DOWN) {
                    ++main_buffer.cur_line;
                } else if (c == KEY_LEFT) {
                    --main_buffer.cur_character;
                } else if (c == KEY_RIGHT) {
                    ++main_buffer.cur_character;
                }
                // EOF
                if (c == 4) break;
            } else if (n == 0) {
                break;
            }

            draw();
        }
        // Do other work here
        usleep(1000);
    }

    FILE * write_file = fopen(file_name, "w");
    if (!write_file) {
        fprintf(stderr, "Failed to write to %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < main_buffer.num_lines; ++i) {
        size_t count = gap_buffer_count(main_buffer.line_bufs[i]);
        gap_buffer_str(main_buffer.line_bufs[i], print_buf);
        fprintf(write_file, "%.*s\n", (int)count, print_buf);
        // deinit
    }

    tb_deinit(&main_buffer);

    if (fclose(write_file)) {
        fprintf(stderr, "Failed to close file %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    return 0;
}
