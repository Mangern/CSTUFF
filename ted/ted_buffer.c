#include <assert.h>
#include <stdlib.h>

#include "ted_buffer.h"

typedef struct ted_buffer_t ted_buffer_t;

static void tb_grow(ted_buffer_t* tb);

void tb_insert_line_after(ted_buffer_t* tb, int line) {
    if (tb->num_lines == tb->capacity) {
        tb_grow(tb);
    }

    for (int i = tb->num_lines - 1; i > line; --i) {
        tb->line_bufs[i+1] = tb->line_bufs[i];
    }

    tb->line_bufs[line+1] = calloc(1, sizeof(struct gap_buffer_t));
    gap_buffer_init(tb->line_bufs[line+1]);

    ++tb->num_lines;
}

void tb_delete_line(ted_buffer_t* tb, int line) {
    assert(0 <= line && line < tb->num_lines);

    gap_buffer_deinit(tb->line_bufs[line]);
    free(tb->line_bufs[line]);

    for (int i = line; i + 1 < tb->num_lines; ++i) {
        tb->line_bufs[i] = tb->line_bufs[i+1];
    }
    --tb->num_lines;
}

void tb_constrain_line_char(ted_buffer_t* tb, int num_rows, int num_cols) {
    assert(tb->num_lines > 0);
    if (tb->cur_line < 0) tb->cur_line = 0;
    if (tb->cur_line < tb->scroll) {
        tb->scroll = tb->cur_line;
    }
    if (tb->cur_line >= tb->num_lines) tb->cur_line = tb->num_lines - 1;
    if (tb->cur_line >= tb->scroll + num_rows) {
        int diff = tb->cur_line - tb->scroll - num_rows + 1;
        tb->scroll += diff;
        if (tb->scroll > tb->num_lines) {
            tb->scroll = tb->num_lines - 1;
        }
    }
    if (tb->cur_character < 0) {
        tb->cur_character = 0;
    }
    // TODO: horizontal scroll
    int count = gap_buffer_count(tb->line_bufs[tb->cur_line]);
    if (tb->cur_character > count)tb->cur_character = count;
}

void tb_fill_from_string(struct ted_buffer_t* tb, char* str, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        // avoid creating a new gap buffer at the end of the file
        if (i + 1 == len && str[i] == '\n') break;

        if (i == 0 || str[i] == '\n') {
            tb_insert_line_after(tb, tb->num_lines - 1);
            if (str[i] == '\n') continue; // don't include newline in buffer
        }
        gap_buffer_gap_insert(tb->line_bufs[tb->num_lines - 1], str[i]);
    }
}

void tb_deinit(struct ted_buffer_t* tb) {
    for (int i = 0; i < tb->num_lines; ++i) {
        gap_buffer_deinit(tb->line_bufs[i]);
        free(tb->line_bufs[i]);
    }
}

// Internal

void tb_grow(ted_buffer_t* tb) {
    size_t new_cap = tb->capacity * 2;

    if (tb->capacity == 0) {
        // for good measure
        tb->line_bufs = 0;
        new_cap = 1024;
    }

    tb->line_bufs = reallocarray(tb->line_bufs, new_cap, sizeof(struct gap_buffer_t*));
}
