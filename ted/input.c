#include "input.h"
#include "dfa.h"
#include "ted_buffer.h"

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
    dfa_add_transition(dfa_normal.root, ':', NORMAL_ENTER_CMD);
    dfa_add_transition(dfa_normal.root, 'i', NORMAL_ENTER_INSERT);
    dfa_add_transition(dfa_normal.root, 'A', NORMAL_ENTER_INSERT_END);
    dfa_add_transition(dfa_normal.root, 'I', NORMAL_ENTER_INSERT_HOME);
    dfa_add_transition(dfa_normal.root, 'o', NORMAL_ENTER_INSERT_DOWN);
    dfa_add_transition(dfa_normal.root, 'O', NORMAL_ENTER_INSERT_UP);

    dfa_node_t *delete = dfa_add_transition(dfa_normal.root, 'd', NORMAL_DELETE);
    delete->final = false;

    dfa_add_transition(delete, 'd', NORMAL_DELETE_LINE);

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

        case NORMAL_DELETE_LINE:
            {
                if (buf->num_lines == 1) {
                    gap_buffer_gap_at(buf->line_bufs[0], 0);
                    gap_buffer_chop_rest(buf->line_bufs[0]);
                } else {
                    tb_delete_line(buf, buf->cur_line);
                }
            }
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
    }

    if (dfa_normal.state->final) {
        dfa_reset(&dfa_normal);
    }
}
