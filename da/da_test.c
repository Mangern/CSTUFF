#include <stdio.h>
#include "da.h"

void da_int_test() {
    int* arr = da_init(int);

    da_append(&arr, 2);
    da_append(&arr, 3);
    da_append(&arr, 4);

    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%d\n", arr[i]);
    }

    da_pop(&arr);
    printf("Size after pop: %zu\n", da_size(arr));

    da_clear(&arr);
    printf("Size after clear: %zu\n", da_size(arr));

    da_deinit(&arr);
}

void da_string_test() {
    char** arr = da_init(char*);

    da_append(&arr, "Hello!");
    da_append(&arr, "World!");
    da_append(&arr, "foo");
    da_append(&arr, "bar");
    da_append(&arr, "baz");

    puts("Contents:");
    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%s ", arr[i]);
    }
    puts("");
    puts("Removing garbage. Remaining items:");
    da_pop(&arr);
    da_pop(&arr);
    da_pop(&arr);
    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%s ", arr[i]);
    }
    puts("");
    printf("Size: %zu\n", da_size(arr));
    da_deinit(&arr);
}

void da_indexof_test() {
    double* arr = da_init(double);

    double x = 0.2;
    double y = 69.0;
    double z = 420.0;

    da_append(&arr, x);
    da_append(&arr, y);

    printf("Index of x: %ld\n", da_indexof(arr, &x));
    printf("Index of y: %ld\n", da_indexof(arr, &y));
    printf("Index of z: %ld\n", da_indexof(arr, &z));
}

void da_resize_test() {
    int* arr = da_init(int);

    da_append(&arr, 1);
    da_append(&arr, 2);
    da_append(&arr, 3);
    da_append(&arr, 4);

    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%d%c", arr[i], " \n"[i==da_size(arr)-1]);
    }

    da_resize(&arr, 2);

    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%d%c", arr[i], " \n"[i==da_size(arr)-1]);
    }

    da_resize(&arr, 8);

    for (size_t i = 0; i < da_size(arr); ++i) {
        printf("%d%c", arr[i], " \n"[i==da_size(arr)-1]);
    }
}

static size_t* test_arr = 0;
void da_test_2() {
    test_arr = da_init(size_t);

    for (size_t i = 0; i <= 128; ++i) {
        da_append(&test_arr, i);
        da_append(&test_arr, i);
        da_append(&test_arr, i);
    }

    for (size_t i = 0; i < da_size(test_arr); ++i) {
        printf("%zu%c", test_arr[i], " \n"[i==da_size(test_arr)-1]);
    }
}

int main() {
    da_int_test();
    da_string_test();
    da_indexof_test();
    da_resize_test();
    da_test_2();
    return 0;
}
