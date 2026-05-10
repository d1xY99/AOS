#include "stdio.h"
#include "stdlib.h"
#include "pthread.h"
#include "assert.h"

static void *sum_worker(void *payload)
{
    size_t *numbers = (size_t *)payload;
    return (void *)(numbers[0] + numbers[1]);
}

static size_t run_sum_case(size_t first, size_t second)
{
    size_t inputs[2] = {first, second};
    pthread_t calculator = 0;
    int create_result = pthread_create(&calculator, NULL, sum_worker, (void *)inputs);
    assert(create_result == 0 && "test_join_collects_multiple_sums: thread creation failed");
    assert(calculator != 0 && "test_join_collects_multiple_sums: calculator id invalid");

    size_t sum_value = 0;
    int join_result = pthread_join(calculator, (void **)&sum_value);
    assert(join_result == 0 && "test_join_collects_multiple_sums: join should succeed");
    assert(sum_value == first + second && "test_join_collects_multiple_sums: wrong sum returned");

    return sum_value;
}

// Simple join that confirms a computed return value is forwarded to the caller.
int test_join_collects_multiple_sums(void)
{
    size_t first_sum = run_sum_case(33, 36);
    assert(first_sum == 69 && "test_join_collects_multiple_sums: first sum mismatch");

    size_t second_sum = run_sum_case(7, 11);
    assert(second_sum == 18 && "test_join_collects_multiple_sums: second sum mismatch");

    void *sentinel = (void *)0xdeadbeef;
    int join_result = pthread_join((pthread_t)0, &sentinel);
    assert(join_result != 0 && "test_join_collects_multiple_sums: joining invalid id should fail");
    assert(sentinel == (void *)0xdeadbeef && "test_join_collects_multiple_sums: sentinel must stay unchanged");

    return 0;
}
