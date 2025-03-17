#ifndef DA_H
#define DA_H

#include <stddef.h>
#include <stdlib.h>

#define DA_DEFAULT_CAPACITY 1
#define DA_META_SIZE     (3)
#define DA_IDX_SIZE      (-DA_META_SIZE+0)
#define DA_IDX_CAP       (-DA_META_SIZE+1)
#define DA_IDX_ELEM_SIZE (-DA_META_SIZE+2)

void* da_init_impl(size_t elem_size);

#define da_init(t) da_init_impl(sizeof(t))

#define da_reserve(arr, new_cap) do {\
    size_t* meta = *(size_t**)(arr);\
    size_t cap = meta[DA_IDX_CAP];\
    size_t elem_size = meta[DA_IDX_ELEM_SIZE];\
    if (cap >= (new_cap))break;\
    cap = (new_cap);\
    (*(arr)) = (void*)((size_t*)realloc(&meta[-DA_META_SIZE], DA_META_SIZE * sizeof(size_t) + cap * elem_size)+DA_META_SIZE);\
    meta = *(size_t**)(arr);\
    meta[DA_IDX_CAP] = cap;\
} while (0);
    

#define da_append(arr, x) do {\
    size_t* meta = *(size_t**)(arr);\
    size_t size = meta[DA_IDX_SIZE];\
    size_t cap  = meta[DA_IDX_CAP];\
    if (size == cap) {\
        da_reserve((arr), cap * 2);\
        meta = *(size_t**)(arr);\
    }\
    (*(arr))[size] = (x);\
    meta[DA_IDX_SIZE]++;\
} while(0);

size_t da_size(void* arr);

// Accept pointer to arr
void da_pop(void* arr_ptr);

// Accept pointer to arr
void da_clear(void* arr_ptr);

void da_deinit(void* arr_ptr);

#endif
