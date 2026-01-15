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

#include "context.h"
#include "command.h"
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

context_t ctx;
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
            } else if (n == 0) {
                break;
            }
        }
        usleep(1000);
    }
}

void get_window_size() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ctx.win_size);
}

void handler_sigwinch(int signo) {
    (void)signo;
    ctx.should_resize = true;
    ctx.should_draw = true;
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

void draw(context_t* ctx) {
    ctx->should_draw = false;

    if (ctx->should_resize) {
        get_window_size();
        ctx->should_resize = false;
    }
    printf("\x1B[?25l"); // hide cursor
    // move home, erase until end
    printf("\x1B[H\x1B[0J");
    int num_draw = ctx->win_size.ws_row - PAD_TOP - PAD_BOT;
    if (ctx->main_buffer.num_lines - ctx->main_buffer.scroll < num_draw) {
        num_draw = ctx->main_buffer.num_lines - ctx->main_buffer.scroll;
    }
    for (int i = 0; i < num_draw; ++i) {
        gap_buffer_t* cur_line = ctx->main_buffer.line_bufs[ctx->main_buffer.scroll + i];
        size_t count = gap_buffer_count(cur_line);
        gap_buffer_str(cur_line, print_buf);
        // move to correct spot
        int row = PAD_TOP + i + 1;
        int col = 1;
        printf("\x1B[%d;%dH", row, col);
        // print line. TODO: pad right? 
        // -2 for ' '
        printf("\x1B[38;5;241m%*d \x1B[0m%.*s\n", PAD_LFT - 1, ctx->main_buffer.scroll + i + 1, (int)count, print_buf);
    }

    // Write status field
    printf("\x1B[%d;%dH", ctx->win_size.ws_row - 1, 1);
    printf("%s", EDITOR_MODE_STR[ctx->editor_mode]);

    if (ctx->editor_mode == MODE_NORMAL || ctx->editor_mode == MODE_INSERT) {
        // move to current location
        printf("\x1B[%d;%dH", PAD_TOP + ctx->main_buffer.cur_line - ctx->main_buffer.scroll + 1, PAD_LFT + ctx->main_buffer.cur_character + 1);
    } else if (ctx->editor_mode == MODE_COMMAND) {
        int count = gap_buffer_count(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line]);
        gap_buffer_str(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line], print_buf);
        printf("\x1B[%d;%dH:%.*s", ctx->win_size.ws_row, 1, count, print_buf);
        printf("\x1B[%d;%dH", ctx->win_size.ws_row, ctx->cmd_buffer.cur_character + 2);
    }
    printf("\x1B[?25h"); // show cursor

    fflush(stdout);
}

void handle_input_normal(context_t* ctx, int c) {
    switch (c) {
        case '$':
            ctx->main_buffer.cur_character = gap_buffer_count(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line]) - 1;
            break;
        case ':':
            ctx->editor_mode = MODE_COMMAND;
            break;
        case '0':
            ctx->main_buffer.cur_character = 0;
            break;
        case 'A':
            ctx->main_buffer.cur_character = gap_buffer_count(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line]);
            ctx->editor_mode = MODE_INSERT;
            break;
        case 'I':
            ctx->main_buffer.cur_character = 0;
            ctx->editor_mode = MODE_INSERT;
            break;
        case 'O':
            tb_insert_line_after(&ctx->main_buffer, ctx->main_buffer.cur_line - 1);
            ctx->main_buffer.cur_character = 0;
            break;
        case 'h':
            ctx->main_buffer.cur_character -= 1;
            break;
        case 'i':
            ctx->editor_mode = MODE_INSERT;
            break;
        case 'j':
            ctx->main_buffer.cur_line += 1;
            break;
        case 'k':
            ctx->main_buffer.cur_line -= 1;
            break;
        case 'l':
            ctx->main_buffer.cur_character += 1;
            break;
        case 'o':
            tb_insert_line_after(&ctx->main_buffer, ctx->main_buffer.cur_line);
            ++ctx->main_buffer.cur_line;
            ctx->main_buffer.cur_character = 0;
            ctx->editor_mode = MODE_INSERT;
            break;
        case 4:
            // TODO: :q
            ctx->should_quit = true;
            break;
    }
}

void handle_input_insert(context_t *ctx, int c) {
    if (buffer_char(c)) {
        gap_buffer_gap_at(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line], ctx->main_buffer.cur_character);
        gap_buffer_gap_insert(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line], c);
        ctx->main_buffer.cur_character += 1;
        return;
    } 

    switch (c) {
        case '\n': 
            {
                tb_insert_line_after(&ctx->main_buffer, ctx->main_buffer.cur_line);
                gap_buffer_concat(
                    ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line+1], 
                    ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line], 
                    ctx->main_buffer.cur_character
                );
                gap_buffer_chop_rest(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line]);
                ++ctx->main_buffer.cur_line;
                ctx->main_buffer.cur_character = 0;
            }
            break;
        case KEY_BACKSPACE: 
            {
                if (ctx->main_buffer.cur_character > 0) {
                    gap_buffer_gap_at(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line], ctx->main_buffer.cur_character);
                    gap_buffer_gap_delete(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line]);
                    --ctx->main_buffer.cur_character;
                } else if (ctx->main_buffer.cur_line > 0) {
                    ctx->main_buffer.cur_character = gap_buffer_count(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line - 1]);

                    gap_buffer_concat(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line - 1], ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line], 0);
                    tb_delete_line(&ctx->main_buffer, ctx->main_buffer.cur_line);

                    --ctx->main_buffer.cur_line;
                }
            }
            break;
        case KEY_TAB: 
            {
                gap_buffer_gap_at(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line], ctx->main_buffer.cur_character);
                for (int i = 0; i < ctx->tabsize; ++i) {
                    gap_buffer_gap_insert(ctx->main_buffer.line_bufs[ctx->main_buffer.cur_line], ' ');
                    ctx->main_buffer.cur_character += 1;
                }
            }
            break;
        case KEY_UP: 
            --ctx->main_buffer.cur_line;
            break;
        case KEY_DOWN:
            ++ctx->main_buffer.cur_line;
            break;
        case KEY_LEFT:
            --ctx->main_buffer.cur_character;
            break;
        case KEY_RIGHT:
            ++ctx->main_buffer.cur_character;
            break;
        case KEY_ESC:
            ctx->editor_mode = MODE_NORMAL;
            break;
    }
}

void handle_input_command(context_t *ctx, int c) {
    if (buffer_char(c)) {
        gap_buffer_gap_at(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line], ctx->cmd_buffer.cur_character);
        gap_buffer_gap_insert(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line], c);
        ctx->cmd_buffer.cur_character += 1;
        return;
    }

    switch (c) {
        case '\n':
            {
                int size = gap_buffer_count(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line]);
                char* cmd_str = malloc(size+1);
                gap_buffer_str(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line], cmd_str);
                cmd_str[size] = 0;
                parse_execute_command(ctx, cmd_str, size);

                // clear and back to normal
                ctx->editor_mode = MODE_NORMAL;
                gap_buffer_gap_at(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line], 0);
                gap_buffer_chop_rest(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line]);
                ctx->cmd_buffer.cur_character = 0;
            }
            break;
        case KEY_ESC:
            ctx->editor_mode = MODE_NORMAL;
            gap_buffer_gap_at(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line], 0);
            gap_buffer_chop_rest(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line]);
            ctx->cmd_buffer.cur_character = 0;
            break;
        case KEY_BACKSPACE: 
            {
                if (ctx->cmd_buffer.cur_character > 0) {
                    gap_buffer_gap_at(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line], ctx->cmd_buffer.cur_character);
                    gap_buffer_gap_delete(ctx->cmd_buffer.line_bufs[ctx->cmd_buffer.cur_line]);
                    --ctx->cmd_buffer.cur_character;
                }
            }
            break;
        case KEY_LEFT:
            --ctx->cmd_buffer.cur_character;
            break;
        case KEY_RIGHT:
            ++ctx->cmd_buffer.cur_character;
            break;
    }
}

int main(int argc, char **argv) {
    options(argc, argv);

    set_conio_terminal_mode();

    setup_resize();

    ctx.should_draw = 1;
    ctx.should_resize = 1;
    ctx.editor_mode = MODE_NORMAL;
    ctx.tabsize = 4;

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
        tb_fill_from_string(&ctx.main_buffer, str, size);
        fclose(read_file);
        free(str);
    }

    assert(ctx.main_buffer.num_lines > 0);

    tb_fill_from_string(&ctx.cmd_buffer, "", 0);

    print_buf = malloc(GAP_BUFFER_SIZE);
    ctx.main_buffer.cur_line = 0;
    ctx.main_buffer.cur_character = 0;

    draw(&ctx);

    while (!ctx.should_quit) {
        if (kbhit()) {
            ctx.should_draw = true; // since we had keyboard action
            int c = 0;
            int n = read(0, &c, 4);
            if (n == 0) continue;

            switch (ctx.editor_mode) {
                case MODE_NORMAL:
                    handle_input_normal(&ctx, c);
                    break;
                case MODE_INSERT:
                    handle_input_insert(&ctx, c);
                    break;
                case MODE_COMMAND:
                    handle_input_command(&ctx, c);
                    break;
            }
            tb_constrain_line_char(&ctx.main_buffer, ctx.win_size.ws_row - PAD_TOP - PAD_BOT, ctx.win_size.ws_col - PAD_LFT - PAD_RGT);
        }

        if (ctx.should_draw) {
            draw(&ctx);
        }

        // Do other work here
        usleep(50);
    }

    FILE * write_file = fopen(file_name, "w");
    if (!write_file) {
        fprintf(stderr, "Failed to write to %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < ctx.main_buffer.num_lines; ++i) {
        size_t count = gap_buffer_count(ctx.main_buffer.line_bufs[i]);
        gap_buffer_str(ctx.main_buffer.line_bufs[i], print_buf);
        fprintf(write_file, "%.*s\n", (int)count, print_buf);
        // deinit
    }

    tb_deinit(&ctx.main_buffer);

    if (fclose(write_file)) {
        fprintf(stderr, "Failed to close file %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    return 0;
}
