#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "context.h"
#include "command.h"
#include "dfa.h"
#include "gap_buffer.h"
#include "input.h"
#include "ted_buffer.h"

typedef struct gap_buffer_t gap_buffer_t;
typedef struct ted_buffer_t ted_buffer_t;

static struct termios orig_termios;
static bool opt_debug = 0;

static const int PAD_TOP = 2;
static const int PAD_LFT = 7;
static const int PAD_RGT = 0;
static const int PAD_BOT = 2;

static const int TREE_SIZE = 20;

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

void draw(context_t* ctx) {
    ted_buffer_t *main_buf = &ctx->cur_buf->buf;
    ted_buffer_t *cmd_buf = &ctx->cmd_buf->buf;
    ted_buffer_t *log_buf = &ctx->log_buf->buf;
    ted_buffer_t *tre_buf = &ctx->tre_buf->buf;
    ctx->should_draw = false;

    if (ctx->should_resize) {
        get_window_size();
        ctx->should_resize = false;
    }
    printf("\x1B[?25l"); // hide cursor
    // move home, erase until end
    printf("\x1B[H\x1B[0J");
    int num_draw = ctx->win_size.ws_row - PAD_TOP - PAD_BOT;
    if (main_buf->num_lines - main_buf->scroll < num_draw) {
        num_draw = main_buf->num_lines - main_buf->scroll;
    }

    int main_col_start = PAD_LFT + 1;

    if (ctx->tree_expanded) {
        main_col_start += TREE_SIZE;
    }

    // Draw top line
    {
        struct bufentry_t *entry = ctx->bufhead;
        printf("\x1B[%d;%dH", 1, main_col_start);
        // Draw filenames
        for (;;) {
            if (entry != ctx->cmd_buf && entry != ctx->log_buf && entry != ctx->tre_buf) {
                if (entry == ctx->cur_buf) {
                    printf("\x1B[48;5;241m");
                }

                printf(" %s ", entry->file_name ? entry->file_name : "[No name]");

                if (entry == ctx->cur_buf) {
                    printf("\x1B[0m");
                }
            }
            if (entry == ctx->buftail) break;
            entry = entry->nxt;
        }

        // hmm

        printf("\x1B[%d;%dH", 2, 1);
        printf("\x1B[38;5;241m");
        for (int i = 0; i < ctx->win_size.ws_col; ++i) {
            printf("─");
        }
        printf("\x1B[0m");
    }

    for (int i = 0; i < num_draw; ++i) {
        gap_buffer_t* cur_line = main_buf->line_bufs[main_buf->scroll + i];
        //int count = gap_buffer_count(cur_line) - main_buf->hscroll;
        //gap_buffer_str(cur_line, print_buf);
        int count = gap_buffer_substr(cur_line, print_buf, main_buf->hscroll, ctx->win_size.ws_col - main_col_start - PAD_RGT);
        // move to correct spot
        int row = PAD_TOP + i + 1;
        int col = main_col_start - PAD_LFT;
        printf("\x1B[%d;%dH", row, col);
        // print line. TODO: pad right? 
        // -2 for ' '
        int relnum = abs(main_buf->scroll + i - main_buf->cur_line);
        if (relnum == 0) {
            relnum = main_buf->scroll + i + 1;
        }
        printf("\x1B[38;5;241m%*d \x1B[0m%.*s\n", PAD_LFT, relnum, (int)count, print_buf);
    }

    if (ctx->tree_expanded) {
        printf("\x1B[38;5;241m");
        for (int i = 0; i < ctx->win_size.ws_row; ++i) {
            printf("\x1B[%d;%dH", 
                1 + i, 
                TREE_SIZE
            );
            if (i == 1) {
                printf("┼");
            } else {
                printf("│");
            }
        }
        printf("\x1B[0m");

        
        int tree_draw = ctx->win_size.ws_row - PAD_TOP - PAD_BOT;
        if (tre_buf->num_lines - tre_buf->scroll < tree_draw) {
            tree_draw = tre_buf->num_lines - tre_buf->scroll;
        }

        for (int i = 0; i < tree_draw; ++i) {
            gap_buffer_t* cur_line = tre_buf->line_bufs[tre_buf->scroll + i];
            int count = gap_buffer_substr(cur_line, print_buf, tre_buf->hscroll, TREE_SIZE - 1);
            int row = PAD_TOP + i + 1;
            int col = 1;
            printf("\x1B[%d;%dH", row, col);
            printf("%.*s", count, print_buf);
        }
    }

    // Write status field
    printf("\x1B[%d;%dH", ctx->win_size.ws_row - 1, 1);
    char *fn_str = ctx->cur_buf->file_name;
    if (fn_str == 0) {
        fn_str = "[No name]";
    }
    printf("%8s \"%s\"", EDITOR_MODE_STR[ctx->editor_mode], fn_str);

    if (ctx->editor_mode == MODE_NORMAL || ctx->editor_mode == MODE_INSERT) {
        // last line of log
        int count = gap_buffer_count(log_buf->line_bufs[log_buf->cur_line]);
        gap_buffer_str(log_buf->line_bufs[log_buf->cur_line], print_buf);

        printf("\x1B[%d;%dH%.*s", ctx->win_size.ws_row, 1, count, print_buf);

        // move to current location
        printf("\x1B[%d;%dH", 
            PAD_TOP + main_buf->cur_line - main_buf->scroll + 1, 
            main_col_start + main_buf->cur_character - main_buf->hscroll + 1
        );
    } else if (ctx->editor_mode == MODE_COMMAND) {
        int count = gap_buffer_count(cmd_buf->line_bufs[cmd_buf->cur_line]);
        gap_buffer_str(cmd_buf->line_bufs[cmd_buf->cur_line], print_buf);
        printf("\x1B[%d;%dH:%.*s", ctx->win_size.ws_row, 1, count, print_buf);
        printf("\x1B[%d;%dH", ctx->win_size.ws_row, cmd_buf->cur_character + 2);
    }
    printf("\x1B[?25h"); // show cursor

    fflush(stdout);
}

int main(int argc, char **argv) {
    options(argc, argv);

    set_conio_terminal_mode();

    setup_resize();

    ctx_init(&ctx);

    init_inputs();

    if (opt_debug) {
        debug_keyboard();
        return 0;
    }
    
    if (optind < argc) {
        char* file_name = argv[optind];
        ++optind; // hmm,

        cmd_edit_file(&ctx, strdup(file_name));
    }
    
    print_buf = malloc(GAP_BUFFER_SIZE);

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
            tb_constrain_line_char(
                &ctx.cur_buf->buf, 
                ctx.win_size.ws_row - PAD_TOP - PAD_BOT, 
                ctx.win_size.ws_col - PAD_LFT - PAD_RGT - (ctx.tree_expanded ? TREE_SIZE : 0));
        }

        if (ctx.should_draw) {
            draw(&ctx);
        }

        // Do other work here
        usleep(50);
    }
    // move home, erase until end
    printf("\x1B[H\x1B[0J");

    // clear memory for fun
    ctx_deinit(&ctx);
    free(print_buf);

    return 0;
}
