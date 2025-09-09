#include <ctype.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>

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
    return (isascii(c) && isprint(c)) || (c == '\n');
}

#define BUF_SIZE (1024*1024)

char buffer[BUF_SIZE];
int buffer_count = 0;

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

    if (read_file) {
        buffer_count = fread(buffer, 1, BUF_SIZE, read_file);
        fclose(read_file);
    } else {
        fprintf(stderr, "Failed to read\n");
        exit(-1);
    }

    while (1) {
        printf("\x1B[H\x1B[0J");
        for (int i = 0; i < buffer_count; ++i) {
            printf("%c", buffer[i]);
        }
        fflush(stdout);
        if (kbhit()) {
            int c = 0;
            int n = read(0, &c, 4);
            if (n > 0) {
                if (buffer_char(c)) {
                    buffer[buffer_count++] = c;
                } else if (c == KEY_BACKSPACE) {
                    if (buffer_count > 0) {
                        --buffer_count;
                    }
                } else if (c == KEY_TAB) {
                    for (int i = 0; i < tabsize; ++i) {
                        buffer[buffer_count++] = ' ';
                    }
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

    fwrite(buffer, buffer_count, 1, write_file);

    if (fclose(write_file)) {
        fprintf(stderr, "Failed to close file %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    return 0;
}
