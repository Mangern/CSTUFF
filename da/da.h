#ifndef CSTUFF_DA_H
#define CSTUFF_DA_H

#include <stddef.h>

#define DA_PUSH_BACK_INT(da, val) { int __tmp = val; da_push_back(da, &__tmp); }
#define DA_AT(da, index, type) (*(type *)da_at(da, index))

typedef struct da_t {
    size_t size;
    size_t capacity;
    size_t elem_size;
    void* buffer;
} da_t;

void  da_init(struct da_t* da, size_t elem_size);
void  da_deinit(struct da_t* da);
void  da_resize(struct da_t* da, size_t size);
void  da_reserve(struct da_t* da, size_t capacity);
void* da_at(struct da_t* da, size_t index);
void  da_push_back(struct da_t* da, void* data);
void  da_pop_back(struct da_t* da);
size_t da_indexof(struct da_t* da, void* element);

#endif
