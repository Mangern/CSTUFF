#ifndef DA_H
#define DA_H

#include <stddef.h>
#include <stdlib.h>

#define DA_DEFAULT_CAPACITY 1
#define DA_META_SIZE     (3)
#define DA_IDX_SIZE      (-DA_META_SIZE+0)
#define DA_IDX_CAP       (-DA_META_SIZE+1)
#define DA_IDX_ELEM_SIZE (-DA_META_SIZE+2)

typedef struct {
    size_t size;
    size_t capacity;
} da_header_t;

void* da_reserve_impl(void*, size_t, size_t);

#define da_header(arr) (((da_header_t*)(arr)) - 1)

#define da_reserve(arr, new_cap) if (!((arr)) || da_header(arr)->capacity < new_cap) {\
    (arr) = da_reserve_impl((arr), new_cap, sizeof(*(arr))); }

#define da_resize(arr, new_size) do {\
    da_reserve(arr, new_size);\
    da_header(arr)->size = new_size;\
    } while (0);

#define da_append(arr, x) do {\
    if (!(arr)) da_reserve(arr, DA_DEFAULT_CAPACITY);\
    if (da_header(arr)->size == da_header(arr)->capacity) {\
        da_reserve(arr, 2 * da_header(arr)->capacity);\
    }\
    (arr)[da_header(arr)->size++] = x;\
    } while (0);

#define da_size(arr) ((arr) ? da_header(arr)->size : 0)

#define da_pop(arr) (arr)[--da_header(arr)->size]

#define da_clear(arr) if (arr) { da_header(arr)->size = 0; }

#define da_deinit(arr) if (arr) { free(da_header(arr)); }

// Elem: pointer to element to find
#define da_indexof(arr, elem) da_indexof_impl((arr), elem, sizeof (*(arr)))

int64_t da_indexof_impl(void* arr, void* elem, size_t elem_size);

#endif

