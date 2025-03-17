#include "da.h"
#include <stdlib.h>
#include <string.h>

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

void da_pop(void* arr_ptr) {
    size_t* meta = *(size_t**)arr_ptr;
    --meta[DA_IDX_SIZE];
}

void da_clear(void* arr_ptr) {
    size_t* meta = *(size_t**)arr_ptr;
    meta[DA_IDX_SIZE] = 0;
}

void da_deinit(void* arr_ptr) {
    size_t* meta = *(size_t**)arr_ptr;
    free(&meta[-DA_META_SIZE]);
}

int64_t da_indexof(void *arr, void* elem) {
    size_t* meta = (size_t*)arr;
    int64_t size = meta[DA_IDX_SIZE];
    size_t elem_size = meta[DA_IDX_ELEM_SIZE];
    char* byte_arr = (char*)arr;
    for (int64_t i = 0; i < size; ++i) {
        if (memcmp(&byte_arr[i*elem_size], elem, elem_size) == 0) return i;
    }
    return -1;
}
