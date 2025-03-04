#include <stdio.h>
#include <assert.h>

#include "da.h"

int main() {
    da_t da;
    da_init(&da, sizeof(int));

    DA_PUSH_BACK_INT(&da, 1);
    DA_PUSH_BACK_INT(&da, 2);
    DA_PUSH_BACK_INT(&da, 3);
    DA_PUSH_BACK_INT(&da, 4);
    DA_PUSH_BACK_INT(&da, 5);

    for (int i = 0; i < da.size; ++i) {
        printf("%d ", *(int*)da_at(&da, i));
    }
    printf("\n");

    da_pop_back(&da);

    for (int i = 0; i < da.size; ++i) {
        printf("%d ", *(int*)da_at(&da, i));
    }
    printf("\n");

    printf("%zu\n", da.capacity);

    int __tmp = 3;
    printf("Index of 3: %zu\n", da_indexof(&da, &__tmp));
    __tmp = 69;
    printf("Index of 69: %zu\n", da_indexof(&da, &__tmp));

    assert(da_indexof(&da, &__tmp) == -1);

    da_deinit(&da);
    return 0;
}
