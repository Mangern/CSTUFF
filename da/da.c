#include "da.h"
#include <stdlib.h>

void* da_init_impl(size_t elem_size) {
    size_t* mem = malloc(DA_META_SIZE * sizeof(size_t) + DA_DEFAULT_CAPACITY * elem_size);

    mem += DA_META_SIZE;

    mem[DA_IDX_SIZE] = 0;
    mem[DA_IDX_CAP] = DA_DEFAULT_CAPACITY;
    mem[DA_IDX_ELEM_SIZE] = elem_size;

    return mem; // array starts here
}

size_t da_size(void* arr) {
    size_t* meta = (size_t*)arr;
    return meta[DA_IDX_SIZE];
}

void da_pop(void* arr) {
    size_t* meta = (size_t*)arr;
    --meta[DA_IDX_SIZE];
}

void da_clear(void* arr) {
    size_t* meta = (size_t*)arr;
    meta[DA_IDX_SIZE] = 0;
}

void da_deinit(void* arr) {
    size_t* meta = (size_t*)arr;
    free(&meta[-DA_META_SIZE]);
}
