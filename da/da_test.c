#include <stdio.h>

#include "da.h"

int main() {
    struct da_t da;
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


    da_deinit(&da);
    return 0;
}
