#include "da.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void* da_reserve_impl(void* arr, size_t capacity, size_t elem_size) {
    da_header_t header = {0};
    da_header_t* old_header = (da_header_t*)arr - 1;
    header.size = arr ? old_header->size : 0;
    header.capacity = capacity;
    void* b;
    if (!arr) {
        b = malloc(capacity * elem_size + sizeof(da_header_t));
    } else {
        b = realloc(old_header, capacity * elem_size + sizeof(da_header_t));
    }
    *(da_header_t*)b = header;
    b = (char*)b + sizeof(da_header_t);
    return b;
}

int64_t da_indexof_impl(void* arr, void* elem, size_t elem_size) {
    if (!arr) return -1;
    da_header_t* header = (da_header_t*)arr - 1;
    char* byte_arr = (char*)arr;
    for (int64_t i = 0; i < header->size; ++i) {
        if (memcmp(&byte_arr[i*elem_size], elem, elem_size) == 0) return i;
    }
    return -1;
}
