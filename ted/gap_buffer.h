#ifndef GAP_BUFFER_H
#define GAP_BUFFER_H

#include <stdint.h>
#include <stddef.h>

#define GAP_BUFFER_SIZE (4096)

struct gap_buffer_t {
    char *buffer;        // some kind of buffer
    size_t end;          // end of useful content
    size_t gap_start;    // start index of gap
    size_t gap_size;     // size of gap
    size_t buffer_size;  // size of buffer 
};

void gap_buffer_init(struct gap_buffer_t *gap_buffer);

/* Collapse the gap if it exists */
void gap_buffer_collapse(struct gap_buffer_t *gap_buffer);

/* Make the gap start at idx */
void gap_buffer_gap_at(struct gap_buffer_t *gap_buffer, size_t idx);

/* Blindly assumes gap exists and inserts a character there */
void gap_buffer_gap_insert(struct gap_buffer_t *gap_buffer, char c);

void gap_buffer_gap_delete(struct gap_buffer_t *gap_buffer);

void gap_buffer_str(struct gap_buffer_t *gap_buffer, char *dst);

void gap_buffer_deinit(struct gap_buffer_t *gap_buffer);

/* How many useful elements are there? */
size_t gap_buffer_count(struct gap_buffer_t *gap_buffer);

#endif // GAP_BUFFER_H
