#ifndef TED_BUFFER_H
#define TED_BUFFER_H

#include "gap_buffer.h"

/*
 * A stateful ted buffer.
 *
 * State:
 * - The buffer contents, represented as a dynamic array of gap buffers.
 * - Cursor line
 * - Cursor character
 */
struct ted_buffer_t {
    struct gap_buffer_t** line_bufs;
    int num_lines;
    int capacity;
    int cur_line;
    int cur_character;
};

void tb_insert_line_after(struct ted_buffer_t* tb, int line);

void tb_delete_line(struct ted_buffer_t* tb, int line);

void tb_constrain_line_char(struct ted_buffer_t* tb);

void tb_fill_from_string(struct ted_buffer_t* tb, char* str, size_t len);

void tb_deinit(struct ted_buffer_t* tb);

#endif // TED_BUFFER_H
