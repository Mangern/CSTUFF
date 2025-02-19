#ifndef CSTUFF_DA_H
#define CSTUFF_DA_H

#include <stddef.h>
#include <stdlib.h>

#define DA_PUSH_BACK_INT(da, val) { int __tmp = val; da_push_back(da, &__tmp); }
#define DA_AT(da, index, type) (*(type *)da_at(da, index))

typedef struct {
    size_t size;
    size_t capacity;
    size_t elem_size;
    void* buffer;
} da_t;

void  da_init(da_t* da, size_t elem_size);
void  da_deinit(da_t* da);
void  da_resize(da_t* da, size_t size);
void  da_reserve(da_t* da, size_t capacity);
void* da_at(da_t* da, size_t index);
void  da_push_back(da_t* da, void* data);
void  da_pop_back(da_t* da);
void  da_clear(da_t* da);
void  da_sort(da_t* da, __compar_fn_t compar);
size_t da_indexof(da_t* da, void* element);

#endif
