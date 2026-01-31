#include "command.h"
#include "input.h"
#include "dfa.h"
#include "ted_buffer.h"

#include <ctype.h>
#include <stdlib.h>

#define BUFFER_CHAR(c) (isascii(c) && isprint(c))

static const int KEY_CTRL_D    = 0x04;
static const int KEY_CTRL_E    = 0x05;
static const int KEY_CTRL_U    = 0x15;
static const int KEY_CTRL_Y    = 0x19;
static const int KEY_TAB       = 0x9;
static const int KEY_ESC       = 0x1b;
static const int KEY_BACKSPACE = 0x7f;
static const int KEY_UP        = 0x415b1b;
static const int KEY_DOWN      = 0x425b1b;
static const int KEY_RIGHT     = 0x435b1b;
static const int KEY_LEFT      = 0x445b1b;
static const int KEY_SHIFT_TAB = 0x5a5b1b;


typedef struct ted_buffer_t ted_buffer_t;
typedef struct dfa_node_t dfa_node_t;

dfa_t dfa_normal;

void init_inputs() {
    dfa_init(&dfa_normal);

    dfa_add_transition(dfa_normal.root, '0', NORMAL_NAV_HOME);
    dfa_add_transition(dfa_normal.root, '$', NORMAL_NAV_END);
    dfa_add_transition(dfa_normal.root, 'h', NORMAL_NAV_LFT);
    dfa_add_transition(dfa_normal.root, 'l', NORMAL_NAV_RGT);
    dfa_add_transition(dfa_normal.root, 'j', NORMAL_NAV_DOWN);
    dfa_add_transition(dfa_normal.root, 'k', NORMAL_NAV_UP);
    dfa_add_transition(dfa_normal.root, 'G', NORMAL_NAV_FILEEND);
    dfa_add_transition(dfa_normal.root, ':', NORMAL_ENTER_CMD);
    dfa_add_transition(dfa_normal.root, 'i', NORMAL_ENTER_INSERT);
    dfa_add_transition(dfa_normal.root, 'A', NORMAL_ENTER_INSERT_END);
    dfa_add_transition(dfa_normal.root, 'I', NORMAL_ENTER_INSERT_HOME);
    dfa_add_transition(dfa_normal.root, 'o', NORMAL_ENTER_INSERT_DOWN);
    dfa_add_transition(dfa_normal.root, 'O', NORMAL_ENTER_INSERT_UP);
    dfa_add_transition(dfa_normal.root, KEY_CTRL_E, NORMAL_SCROLL_DOWN);
    dfa_add_transition(dfa_normal.root, KEY_CTRL_Y, NORMAL_SCROLL_UP);
    dfa_add_transition(dfa_normal.root, KEY_CTRL_D, NORMAL_PAGE_DOWN);
    dfa_add_transition(dfa_normal.root, KEY_CTRL_U, NORMAL_PAGE_UP);

    dfa_node_t *delete = dfa_add_transition(dfa_normal.root, 'd', NORMAL_D);
    delete->final = false;

    dfa_add_transition(delete, 'd', NORMAL_D_LINE);

    dfa_node_t *go = dfa_add_transition(dfa_normal.root, 'g', NORMAL_G);
    go->final = false;
    dfa_add_transition(go, 'g', NORMAL_G_HOME);

    dfa_node_t *leader = dfa_add_transition(dfa_normal.root, ' ', NORMAL_LEADER);
    leader->final = false;

    dfa_add_transition(leader, '1', NORMAL_EDIT_1);
    dfa_add_transition(leader, '2', NORMAL_EDIT_2);
    dfa_add_transition(leader, '3', NORMAL_EDIT_3);
    dfa_add_transition(leader, '4', NORMAL_EDIT_4);
    dfa_add_transition(leader, '5', NORMAL_EDIT_5);
    dfa_add_transition(leader, '6', NORMAL_EDIT_6);
    dfa_add_transition(leader, '7', NORMAL_EDIT_7);
    dfa_add_transition(leader, '8', NORMAL_EDIT_8);
    dfa_add_transition(leader, '9', NORMAL_EDIT_9);

    dfa_add_transition(leader, KEY_TAB, NORMAL_OPEN_TREE);
    dfa_add_transition(leader, KEY_SHIFT_TAB, NORMAL_CLOSE_TREE);
}

void handle_input_normal(context_t* ctx, int c) {
    ted_buffer_t *buf = &ctx->cur_buf->buf;
    dfa_enter(&dfa_normal, c);

    switch (dfa_normal.state->id) {
        case NORMAL_NAV_HOME:
            buf->cur_character = 0;
            break;
        case NORMAL_NAV_END:
            buf->cur_character = gap_buffer_count(buf->line_bufs[buf->cur_line]) - 1;
            break;
        case NORMAL_ENTER_CMD:
            ctx->editor_mode = MODE_COMMAND;
            break;
        case NORMAL_ENTER_INSERT_END:
            buf->cur_character = gap_buffer_count(buf->line_bufs[buf->cur_line]);
            ctx->editor_mode = MODE_INSERT;
            break;
        case NORMAL_ENTER_INSERT_HOME:
            // TODO: skip whitespace
            buf->cur_character = 0;
            ctx->editor_mode = MODE_INSERT;
            break;
        case NORMAL_ENTER_INSERT_UP:
            tb_insert_line_after(buf, buf->cur_line - 1);
            buf->cur_character = 0;
            ctx->editor_mode = MODE_INSERT;
            break;
        case NORMAL_NAV_LFT:
            buf->cur_character -= 1;
            break;
        case NORMAL_NAV_FILEEND:
            buf->cur_line = buf->num_lines - 1;
            break;
        case NORMAL_ENTER_INSERT:
            ctx->editor_mode = MODE_INSERT;
            break;
        case NORMAL_NAV_DOWN:
            buf->cur_line += 1;
            break;
        case NORMAL_NAV_UP:
            buf->cur_line -= 1;
            break;
        case NORMAL_NAV_RGT:
            buf->cur_character += 1;
            break;
        case NORMAL_ENTER_INSERT_DOWN:
            tb_insert_line_after(buf, buf->cur_line);
            ++buf->cur_line;
            buf->cur_character = 0;
            ctx->editor_mode = MODE_INSERT;
            break;
        case NORMAL_SCROLL_UP:
            {
                if (buf->scroll > 0) {
                    buf->scroll -= 1;
                }
            }
            break;
        case NORMAL_SCROLL_DOWN:
            {
                if (buf->scroll + 1 < buf->num_lines) {
                    buf->scroll += 1;
                    if (buf->cur_line < buf->scroll) {
                        buf->cur_line = buf->scroll;
                    }
                }
            }
            break;
        case NORMAL_PAGE_UP:
            {
                int njmp = ctx->win_size.ws_row / 2;
                buf->scroll -= njmp;
                buf->cur_line -= njmp;
                if (buf->scroll < 0)
                    buf->scroll = 0;
            }
            break;
        case NORMAL_PAGE_DOWN:
            {
                int njmp = ctx->win_size.ws_row / 2;
                buf->scroll += njmp;
                if (buf->scroll >= buf->num_lines)
                    buf->scroll = buf->num_lines - 1;
                if (buf->cur_line < buf->scroll) {
                    buf->cur_line = buf->scroll;
                }
            }
            break;
        case NORMAL_D_LINE:
            {
                if (buf->num_lines == 1) {
                    gap_buffer_gap_at(buf->line_bufs[0], 0);
                    gap_buffer_chop_rest(buf->line_bufs[0]);
                } else {
                    tb_delete_line(buf, buf->cur_line);
                }
            }
            break;

        case NORMAL_G_HOME:
            buf->cur_line = 0;
            break;

        // Probably a bit risky to do it like this but...
        case NORMAL_EDIT_1:
        case NORMAL_EDIT_2:
        case NORMAL_EDIT_3:
        case NORMAL_EDIT_4:
        case NORMAL_EDIT_5:
        case NORMAL_EDIT_6:
        case NORMAL_EDIT_7:
        case NORMAL_EDIT_8:
        case NORMAL_EDIT_9:
            {
                int idx = dfa_normal.state->id  - NORMAL_EDIT_1;
                struct bufentry_t *entry = ctx_get_editor_buf(ctx, idx);
                if (entry == 0) {
                    ctx_log(ctx, "No such buffer!");
                } else {
                    ctx->cur_buf = entry;
                }
            }
            break;

        case NORMAL_OPEN_TREE:
            {
                struct cmd_result_t result = cmd_expand_tree(ctx);
                if (result.err) {
                    ctx_log(ctx, result.emsg);
                }
            }
            break;
        case NORMAL_CLOSE_TREE:
            {
                struct cmd_result_t result = cmd_collapse_tree(ctx);
                if (result.err) {
                    ctx_log(ctx, result.emsg);
                }
            }
            break;
    }

    if (dfa_normal.state->final) {
        dfa_reset(&dfa_normal);
    }
}

void handle_input_insert(context_t *ctx, int c) {
    ted_buffer_t *buf = &ctx->cur_buf->buf;
    if (BUFFER_CHAR(c)) {
        gap_buffer_gap_at(buf->line_bufs[buf->cur_line], buf->cur_character);
        gap_buffer_gap_insert(buf->line_bufs[buf->cur_line], c);
        buf->cur_character += 1;
        return;
    } 

    switch (c) {
        case '\n': 
            {
                tb_insert_line_after(buf, buf->cur_line);
                gap_buffer_concat(
                    buf->line_bufs[buf->cur_line+1], 
                    buf->line_bufs[buf->cur_line], 
                    buf->cur_character
                );
                gap_buffer_chop_rest(buf->line_bufs[buf->cur_line]);
                ++buf->cur_line;
                buf->cur_character = 0;
            }
            break;
        case KEY_BACKSPACE: 
            {
                if (buf->cur_character > 0) {
                    gap_buffer_gap_at(buf->line_bufs[buf->cur_line], buf->cur_character);
                    gap_buffer_gap_delete(buf->line_bufs[buf->cur_line]);
                    --buf->cur_character;
                } else if (buf->cur_line > 0) {
                    buf->cur_character = gap_buffer_count(buf->line_bufs[buf->cur_line - 1]);

                    gap_buffer_concat(buf->line_bufs[buf->cur_line - 1], buf->line_bufs[buf->cur_line], 0);
                    tb_delete_line(buf, buf->cur_line);

                    --buf->cur_line;
                }
            }
            break;
        case KEY_TAB: 
            {
                gap_buffer_gap_at(buf->line_bufs[buf->cur_line], buf->cur_character);
                for (int i = 0; i < ctx->tabsize; ++i) {
                    gap_buffer_gap_insert(buf->line_bufs[buf->cur_line], ' ');
                    buf->cur_character += 1;
                }
            }
            break;
        case KEY_UP: 
            --buf->cur_line;
            break;
        case KEY_DOWN:
            ++buf->cur_line;
            break;
        case KEY_LEFT:
            --buf->cur_character;
            break;
        case KEY_RIGHT:
            ++buf->cur_character;
            break;
        case KEY_ESC:
            ctx->editor_mode = MODE_NORMAL;
            break;
    }
}

void handle_input_command(context_t *ctx, int c) {
    ted_buffer_t *buf = &ctx->cmd_buf->buf;
    if (BUFFER_CHAR(c)) {
        gap_buffer_gap_at(buf->line_bufs[buf->cur_line], buf->cur_character);
        gap_buffer_gap_insert(buf->line_bufs[buf->cur_line], c);
        buf->cur_character += 1;
        return;
    }

    switch (c) {
        case '\n':
            {
                int size = gap_buffer_count(buf->line_bufs[buf->cur_line]);
                char* cmd_str = malloc(size+1);
                gap_buffer_str(buf->line_bufs[buf->cur_line], cmd_str);
                cmd_str[size] = 0;
                struct cmd_result_t res = parse_execute_command(ctx, cmd_str, size);
                free(cmd_str);

                if (res.err) {
                    ctx_log(ctx, res.emsg);
                }

                // clear and back to normal
                ctx->editor_mode = MODE_NORMAL;
                gap_buffer_gap_at(buf->line_bufs[buf->cur_line], 0);
                gap_buffer_chop_rest(buf->line_bufs[buf->cur_line]);
                buf->cur_character = 0;
            }
            break;
        case KEY_ESC:
            ctx->editor_mode = MODE_NORMAL;
            gap_buffer_gap_at(buf->line_bufs[buf->cur_line], 0);
            gap_buffer_chop_rest(buf->line_bufs[buf->cur_line]);
            buf->cur_character = 0;
            break;
        case KEY_BACKSPACE: 
            {
                if (buf->cur_character > 0) {
                    gap_buffer_gap_at(buf->line_bufs[buf->cur_line], buf->cur_character);
                    gap_buffer_gap_delete(buf->line_bufs[buf->cur_line]);
                    --buf->cur_character;
                }
            }
            break;
        case KEY_LEFT:
            --buf->cur_character;
            break;
        case KEY_RIGHT:
            ++buf->cur_character;
            break;
    }
}
