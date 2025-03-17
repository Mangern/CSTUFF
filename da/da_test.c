#include <stdio.h>
#include "da.h"

void da_int_test() {
    int* arr = da_init(int);

    da_append(arr, 2);
    da_append(arr, 3);
    da_append(arr, 4);

    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%d\n", arr[i]);
    }

    da_pop(arr);
    printf("Size after pop: %zu\n", da_size(arr));

    da_clear(arr);
    printf("Size after clear: %zu\n", da_size(arr));

    da_deinit(arr);
}

void da_string_test() {
    char** arr = da_init(char*);

    da_append(arr, "Hello!");
    da_append(arr, "World!");
    da_append(arr, "foo");
    da_append(arr, "bar");
    da_append(arr, "baz");

    puts("Contents:");
    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%s ", arr[i]);
    }
    puts("");
    puts("Removing garbage. Remaining items:");
    da_pop(arr);
    da_pop(arr);
    da_pop(arr);
    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%s ", arr[i]);
    }
    puts("");
    printf("Size: %zu\n", da_size(arr));
    da_deinit(arr);
}

int main() {
    da_int_test();
    da_string_test();
    return 0;
}
