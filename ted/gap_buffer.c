#include "gap_buffer.h"

#include <stdlib.h>
#include <string.h>

void gap_buffer_init(struct gap_buffer_t *gb) {
    gb->buffer = malloc(GAP_BUFFER_SIZE);
    gb->buffer_size = GAP_BUFFER_SIZE;
    gb->end         = GAP_BUFFER_SIZE;
    gb->gap_start   = 0;
    gb->gap_size    = GAP_BUFFER_SIZE;
}

void gap_buffer_collapse(struct gap_buffer_t *gb) {
    if (gb->gap_size == 0) return;

    size_t gap_end = gb->gap_start + gb->gap_size;
    size_t n_move = gb->buffer_size - gap_end;

    if (n_move != 0) {
        memmove(gb->buffer + gb->gap_start, gb->buffer + gap_end, n_move);
    }
    gb->gap_size = 0;
    gb->end = gb->gap_start + n_move;
}

/*
 * Note: idx should not care about a gap existing
 */
void gap_buffer_gap_at(struct gap_buffer_t *gb, size_t idx) {
    /*
     * gap_at(3)
     *
     * abcdef
     *    ^idx
     *
     *  -> abc             ..def
     *        ^idx              ^end
     */
    gap_buffer_collapse(gb);

    if (idx >= gb->end) {
        gb->gap_start = gb->end;
        gb->gap_size = gb->buffer_size - gb->gap_start;
        gb->end = gb->buffer_size;
        return;
    }

    size_t n_move = gb->end - idx;
    size_t dst_idx = gb->buffer_size - n_move;

    memmove(gb->buffer + dst_idx, gb->buffer + idx, n_move);
    gb->gap_start = idx;
    gb->gap_size = dst_idx - idx;
    gb->end = gb->buffer_size;
}

void gap_buffer_gap_insert(struct gap_buffer_t *gb, char c) {
    gb->buffer[gb->gap_start++] = c;
    --gb->gap_size;
}

void gap_buffer_str(struct gap_buffer_t *gb, char *dst) {
    memmove(dst, gb->buffer, gb->gap_start);

    if (gb->gap_size == 0) return;

    size_t gap_end = gb->gap_start + gb->gap_size;
    if (gb->end == gap_end) return;

    memmove(dst + gb->gap_start, gb->buffer + gap_end, gb->end - gap_end);
}

void gap_buffer_deinit(struct gap_buffer_t *gb) {
    if (gb->buffer) {
        free(gb->buffer);
        gb->buffer = 0;
    }
}
