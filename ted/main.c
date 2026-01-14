#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "gap_buffer.h"
#include "ted_buffer.h"

typedef struct gap_buffer_t gap_buffer_t;
typedef struct ted_buffer_t ted_buffer_t;

static struct termios orig_termios;
static bool opt_debug = 0;

static const int KEY_LEFT      = 0x445b1b;
static const int KEY_UP        = 0x415b1b;
static const int KEY_DOWN      = 0x425b1b;
static const int KEY_RIGHT     = 0x435b1b;
static const int KEY_BACKSPACE = 0x7f;
static const int KEY_ESC       = 0x1b;
static const int KEY_TAB       = 0x9;

static const int PAD_TOP = 0;
static const int PAD_LFT = 7;
static const int PAD_RGT = 0;
static const int PAD_BOT = 2;

static int tabsize = 4;

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND
} editor_mode_t;

const char* EDITOR_MODE_STR[] = {
    "NORMAL",
    "INSERT",
    "CMD"
};

// state
// buffer does not contain trailing newline
ted_buffer_t main_buffer;
ted_buffer_t cmd_buffer;
char* print_buf;
editor_mode_t editor_mode = MODE_NORMAL;
bool should_draw = true;
bool should_resize = true;
bool should_quit = false;
struct winsize win_size;

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
            } else if (n == 0) {
                break;
            }
        }
        usleep(1000);
    }
}

void get_window_size() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win_size);
}

void handler_sigwinch(int signo) {
    (void)signo;
    should_resize = true;
    should_draw = true;
}

void setup_resize() {
    signal(SIGWINCH, handler_sigwinch);
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
    should_draw = false;

    if (should_resize) {
        get_window_size();
        should_resize = false;
    }
    printf("\x1B[?25l"); // hide cursor
    // move home, erase until end
    printf("\x1B[H\x1B[0J");
    int num_draw = win_size.ws_row - PAD_TOP - PAD_BOT;
    if (main_buffer.num_lines - main_buffer.scroll < num_draw) {
        num_draw = main_buffer.num_lines - main_buffer.scroll;
    }
    for (int i = 0; i < num_draw; ++i) {
        gap_buffer_t* cur_line = main_buffer.line_bufs[main_buffer.scroll + i];
        size_t count = gap_buffer_count(cur_line);
        gap_buffer_str(cur_line, print_buf);
        // move to correct spot
        int row = PAD_TOP + i + 1;
        int col = 1;
        printf("\x1B[%d;%dH", row, col);
        // print line. TODO: pad right? 
        // -2 for ' '
        printf("\x1B[38;5;241m%*d \x1B[38;5;255m%.*s\n", PAD_LFT - 1, main_buffer.scroll + i + 1, (int)count, print_buf);
    }

    // Write status field
    printf("\x1B[%d;%dH", win_size.ws_row - 1, 1);
    printf("%s", EDITOR_MODE_STR[editor_mode]);

    if (editor_mode == MODE_NORMAL || editor_mode == MODE_INSERT) {
        // move to current location
        printf("\x1B[%d;%dH", PAD_TOP + main_buffer.cur_line - main_buffer.scroll + 1, PAD_LFT + main_buffer.cur_character + 1);
    } else if (editor_mode == MODE_COMMAND) {
        int count = gap_buffer_count(cmd_buffer.line_bufs[cmd_buffer.cur_line]);
        gap_buffer_str(cmd_buffer.line_bufs[cmd_buffer.cur_line], print_buf);
        printf("\x1B[%d;%dH:%.*s", win_size.ws_row, 1, count, print_buf);
        printf("\x1B[%d;%dH", win_size.ws_row, cmd_buffer.cur_character + 2);
    }
    printf("\x1B[?25h"); // show cursor

    fflush(stdout);
}

void handle_input_normal(int c) {
    switch (c) {
        case '$':
            main_buffer.cur_character = gap_buffer_count(main_buffer.line_bufs[main_buffer.cur_line]) - 1;
            break;
        case ':':
            editor_mode = MODE_COMMAND;
            break;
        case '0':
            main_buffer.cur_character = 0;
            break;
        case 'A':
            main_buffer.cur_character = gap_buffer_count(main_buffer.line_bufs[main_buffer.cur_line]);
            editor_mode = MODE_INSERT;
            break;
        case 'I':
            main_buffer.cur_character = 0;
            editor_mode = MODE_INSERT;
            break;
        case 'O':
            tb_insert_line_after(&main_buffer, main_buffer.cur_line - 1);
            main_buffer.cur_character = 0;
            break;
        case 'h':
            main_buffer.cur_character -= 1;
            break;
        case 'i':
            editor_mode = MODE_INSERT;
            break;
        case 'j':
            main_buffer.cur_line += 1;
            break;
        case 'k':
            main_buffer.cur_line -= 1;
            break;
        case 'l':
            main_buffer.cur_character += 1;
            break;
        case 'o':
            tb_insert_line_after(&main_buffer, main_buffer.cur_line);
            ++main_buffer.cur_line;
            main_buffer.cur_character = 0;
            editor_mode = MODE_INSERT;
            break;
        case 4:
            // TODO: :q
            should_quit = true;
            break;
    }
}

void handle_input_insert(int c) {
    if (buffer_char(c)) {
        gap_buffer_gap_at(main_buffer.line_bufs[main_buffer.cur_line], main_buffer.cur_character);
        gap_buffer_gap_insert(main_buffer.line_bufs[main_buffer.cur_line], c);
        main_buffer.cur_character += 1;
        return;
    } 

    switch (c) {
        case '\n': 
            {
                tb_insert_line_after(&main_buffer, main_buffer.cur_line);
                gap_buffer_concat(
                    main_buffer.line_bufs[main_buffer.cur_line+1], 
                    main_buffer.line_bufs[main_buffer.cur_line], 
                    main_buffer.cur_character
                );
                gap_buffer_chop_rest(main_buffer.line_bufs[main_buffer.cur_line]);
                ++main_buffer.cur_line;
                main_buffer.cur_character = 0;
            }
            break;
        case KEY_BACKSPACE: 
            {
                if (main_buffer.cur_character > 0) {
                    gap_buffer_gap_at(main_buffer.line_bufs[main_buffer.cur_line], main_buffer.cur_character);
                    gap_buffer_gap_delete(main_buffer.line_bufs[main_buffer.cur_line]);
                    --main_buffer.cur_character;
                } else if (main_buffer.cur_line > 0) {
                    main_buffer.cur_character = gap_buffer_count(main_buffer.line_bufs[main_buffer.cur_line - 1]);

                    gap_buffer_concat(main_buffer.line_bufs[main_buffer.cur_line - 1], main_buffer.line_bufs[main_buffer.cur_line], 0);
                    tb_delete_line(&main_buffer, main_buffer.cur_line);

                    --main_buffer.cur_line;
                }
            }
            break;
        case KEY_TAB: 
            {
                gap_buffer_gap_at(main_buffer.line_bufs[main_buffer.cur_line], main_buffer.cur_character);
                for (int i = 0; i < tabsize; ++i) {
                    gap_buffer_gap_insert(main_buffer.line_bufs[main_buffer.cur_line], ' ');
                    main_buffer.cur_character += 1;
                }
            }
            break;
        case KEY_UP: 
            --main_buffer.cur_line;
            break;
        case KEY_DOWN:
            ++main_buffer.cur_line;
            break;
        case KEY_LEFT:
            --main_buffer.cur_character;
            break;
        case KEY_RIGHT:
            ++main_buffer.cur_character;
            break;
        case KEY_ESC:
            editor_mode = MODE_NORMAL;
            break;
    }
}

void handle_input_command(int c) {
    if (buffer_char(c)) {
        gap_buffer_gap_at(cmd_buffer.line_bufs[cmd_buffer.cur_line], cmd_buffer.cur_character);
        gap_buffer_gap_insert(cmd_buffer.line_bufs[cmd_buffer.cur_line], c);
        cmd_buffer.cur_character += 1;
        return;
    }

    switch (c) {
        case KEY_ESC:
            editor_mode = MODE_NORMAL;
            gap_buffer_gap_at(cmd_buffer.line_bufs[cmd_buffer.cur_line], 0);
            gap_buffer_chop_rest(cmd_buffer.line_bufs[cmd_buffer.cur_line]);
            cmd_buffer.cur_character = 0;
            break;
        case KEY_BACKSPACE: 
            {
                if (cmd_buffer.cur_character > 0) {
                    gap_buffer_gap_at(cmd_buffer.line_bufs[cmd_buffer.cur_line], cmd_buffer.cur_character);
                    gap_buffer_gap_delete(cmd_buffer.line_bufs[cmd_buffer.cur_line]);
                    --cmd_buffer.cur_character;
                }
            }
            break;
        case KEY_LEFT:
            --cmd_buffer.cur_character;
            break;
        case KEY_RIGHT:
            ++cmd_buffer.cur_character;
            break;
    }
}

int main(int argc, char **argv) {
    options(argc, argv);

    set_conio_terminal_mode();

    setup_resize();

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

    tb_fill_from_string(&cmd_buffer, "", 0);

    print_buf = malloc(GAP_BUFFER_SIZE);
    main_buffer.cur_line = 0;
    main_buffer.cur_character = 0;

    draw();

    while (!should_quit) {
        if (kbhit()) {
            should_draw = true; // since we had keyboard action
            int c = 0;
            int n = read(0, &c, 4);
            if (n == 0) continue;

            switch (editor_mode) {
                case MODE_NORMAL:
                    handle_input_normal(c);
                    break;
                case MODE_INSERT:
                    handle_input_insert(c);
                    break;
                case MODE_COMMAND:
                    handle_input_command(c);
                    break;
            }
            tb_constrain_line_char(&main_buffer, win_size.ws_row - PAD_TOP - PAD_BOT, win_size.ws_col - PAD_LFT - PAD_RGT);
        }

        if (should_draw) {
            draw();
        }

        // Do other work here
        usleep(50);
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
