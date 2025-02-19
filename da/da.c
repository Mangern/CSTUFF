#include "da.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void  da_init(da_t* da, size_t elem_size) {
    da->size      = 0;
    da->capacity  = 1;
    da->elem_size = elem_size;
    da->buffer    = malloc(elem_size);
}

void  da_deinit(da_t* da) {
    free(da->buffer);
}

void  da_resize(da_t* da, size_t size) {
    if (da->capacity < size) {
        da_reserve(da, size);
    }

    da->size = size;
}

void  da_reserve(da_t* da, size_t capacity) {
    if (capacity <= da->capacity) {
        // TODO: is it bad?
        return;
    }

    void* new_buffer = malloc(da->elem_size * capacity);
    memcpy(new_buffer, da->buffer, da->elem_size * da->capacity);

    free(da->buffer);
    da->buffer = new_buffer;

    da->capacity = capacity;
}

void* da_at(da_t* da, size_t index) {
    char* buffer = da->buffer;
    size_t offset_bytes = index * da->elem_size;
    return &buffer[offset_bytes];
}

void  da_push_back(da_t* da, void* data) {
    if (da->size == da->capacity) {
        // TODO: is 2 optimal?
        da_reserve(da, 2 * da->capacity);
    }

    char* buffer = da->buffer;
    size_t offset_bytes = da->size * da->elem_size;

    // TODO: libc call necessary?
    memcpy(&buffer[offset_bytes], data, da->elem_size);

    da->size += 1;
}

void da_pop_back(da_t* da) {
    assert(da->size > 0);

    // TODO: should we free at some point?

    da->size -= 1;
}

void  da_clear(da_t* da) {
    da->size = 0;
}

/*
 * Wrapper for qsort.
 */
void  da_sort(da_t* da, __compar_fn_t compar) {
    qsort(da->buffer, da->size, da->elem_size, compar);
}

size_t da_indexof(da_t* da, void* element) {
    for (int i = 0; i < da->size; ++i) {
        if (memcmp(da_at(da, i), element, da->elem_size) == 0)return i;
    }
    return -1;
}
