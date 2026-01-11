#include "gap_buffer.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <sys/time.h>

char* curr_test;
bool curr_test_fail = 0;
int curr_test_fail_line;
char* curr_test_err;
size_t num_attempted;
size_t num_ok;

#define TEST_GROUP(f) void f() { \
    printf("===== "#f" =====\n");

#define TEST_START(s) { curr_test = s; curr_test_fail = false; curr_test_err = 0; ++num_attempted; printf("\x1b[0m"); } do {
#define TEST_END } while (0); if (!curr_test_fail) { printf("%-23s\x1b[1;32mPASS\n", curr_test); ++num_ok; } else if (curr_test_err != 0) { printf("\x1b[1;31m%s:%d: %s\n", curr_test, curr_test_fail_line, curr_test_err); } else { printf("\x1b[1;31m%s:%d: Assertion failed.\n", curr_test, curr_test_fail_line); }
#define TEST_ASSERT(expr) if (!(expr)) { curr_test_fail = 1; curr_test_fail_line = __LINE__; break; }
// TODO: Format str?
#define TEST_ASSERT_MSG(expr, msg) if (!(expr)) { curr_test_fail = 1; curr_test_fail_line = __LINE__; curr_test_err = (msg); break; }

TEST_GROUP(gap_buffer_test)

    struct gap_buffer_t gap_buffer;


    TEST_START("init");
        gap_buffer_init(&gap_buffer);
        TEST_ASSERT(gap_buffer.buffer != NULL);
    TEST_END;

    TEST_START("gap_at_start");
        gap_buffer_gap_at(&gap_buffer, 0);

        TEST_ASSERT_MSG(gap_buffer.gap_start == 0, "gap_start did not match expected");
        TEST_ASSERT_MSG(gap_buffer.gap_size == GAP_BUFFER_SIZE, "gap_size did not match expected");
        TEST_ASSERT(gap_buffer_count(&gap_buffer) == 0);
    TEST_END;

    TEST_START("gap_insert_start");
        gap_buffer_gap_insert(&gap_buffer, 'f');
        gap_buffer_gap_insert(&gap_buffer, 'o');
        gap_buffer_gap_insert(&gap_buffer, 'b');
        gap_buffer_gap_insert(&gap_buffer, 'a');

        TEST_ASSERT(gap_buffer_count(&gap_buffer) == 4);
        TEST_ASSERT(memcmp(gap_buffer.buffer, "fo", 2) == 0);
    TEST_END;

    TEST_START("gap_at_middle");
        gap_buffer_gap_at(&gap_buffer, 2);

        TEST_ASSERT(gap_buffer.gap_start + gap_buffer.gap_size == gap_buffer.buffer_size - 2);

        gap_buffer_gap_insert(&gap_buffer, 'o');
        gap_buffer_gap_insert(&gap_buffer, ' ');

        TEST_ASSERT(memcmp(gap_buffer.buffer, "foo ", 4) == 0);

        gap_buffer_gap_at(&gap_buffer, 6);

        gap_buffer_gap_insert(&gap_buffer, 'r');
        gap_buffer_gap_insert(&gap_buffer, ' ');
        gap_buffer_gap_insert(&gap_buffer, 'b');
        gap_buffer_gap_insert(&gap_buffer, 'a');
        gap_buffer_gap_insert(&gap_buffer, 'z');

        TEST_ASSERT(gap_buffer_count(&gap_buffer) == 11);
        TEST_ASSERT(memcmp(gap_buffer.buffer, "foo bar baz", 11) == 0);
    TEST_END;

    TEST_START("str");

        char *result = malloc(100);

        gap_buffer_str(&gap_buffer, result);

        TEST_ASSERT(memcmp(result, "foo bar baz", 11) == 0);

        memset(result, 0, 100);

        gap_buffer_gap_at(&gap_buffer, 0);
        gap_buffer_str(&gap_buffer, result);
        TEST_ASSERT(memcmp(result, "foo bar baz", 11) == 0);

        memset(result, 0, 100);

        gap_buffer_gap_at(&gap_buffer, 2);
        gap_buffer_str(&gap_buffer, result);

        TEST_ASSERT(memcmp(result, "foo bar baz", 11) == 0);

        free(result);
    TEST_END;

    TEST_START("misc");

        gap_buffer_gap_at(&gap_buffer, 7);

        gap_buffer_gap_insert(&gap_buffer, ' ');
        gap_buffer_gap_insert(&gap_buffer, 's');
        gap_buffer_gap_insert(&gap_buffer, 'o');
        gap_buffer_gap_insert(&gap_buffer, 'm');
        gap_buffer_gap_insert(&gap_buffer, 'e');

        char *result = malloc(100);
        gap_buffer_str(&gap_buffer, result);

        TEST_ASSERT(memcmp(result, "foo bar some baz", 16) == 0);

        gap_buffer_gap_at(&gap_buffer, 0);
        gap_buffer_gap_insert(&gap_buffer, 't');
        gap_buffer_gap_insert(&gap_buffer, 'e');
        gap_buffer_gap_insert(&gap_buffer, 's');
        gap_buffer_gap_insert(&gap_buffer, 't');
        gap_buffer_gap_insert(&gap_buffer, ':');
        gap_buffer_gap_insert(&gap_buffer, ' ');

        gap_buffer_gap_at(&gap_buffer, 10);

        gap_buffer_str(&gap_buffer, result);

        TEST_ASSERT(gap_buffer_count(&gap_buffer) == 22);
        TEST_ASSERT(memcmp(result, "test: foo bar some baz", 22) == 0);

        free(result);

    TEST_END;


    TEST_START("deinit");
        gap_buffer_deinit(&gap_buffer);
        TEST_ASSERT(gap_buffer.buffer == NULL);
    TEST_END;
}

#define WALLTIME(t) (((double)(t).tv_sec + 1e-6 * (double)(t).tv_usec)*1000.0)

int main(void) {
    struct timeval t_start, t_end;

    gettimeofday(&t_start, NULL);

    gap_buffer_test();

    gettimeofday(&t_end, NULL);

    puts("\x1b[0m");

    printf("Done testing, \x1b[1;32m %zu Passed\x1b[0m, \x1b[1;3%dm%zu Failed\x1b[0m in %.2f ms\n", num_ok, num_ok < num_attempted, num_attempted - num_ok, WALLTIME(t_end) - WALLTIME(t_start));
}
