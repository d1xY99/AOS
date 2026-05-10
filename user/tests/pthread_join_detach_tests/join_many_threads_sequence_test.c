#include "pthread.h"
#include "assert.h"
#include "stdio.h"

static void *bump_and_return(void *ignored)
{
    (void)ignored;
    int local_value = 4;
    local_value++;
    return (void *)(size_t)local_value;
}

static size_t run_worker_sequence(size_t repetitions)
{
    const size_t expected = 5;
    size_t aggregate = 0;
    size_t completed = 0;

    while (completed < repetitions)
    {
        pthread_t worker = 0;
        int create_result = pthread_create(&worker, NULL, bump_and_return, NULL);
        assert(create_result == 0 && "test_join_loop_multiple_threads: worker creation failed");
        assert(worker != 0 && "test_join_loop_multiple_threads: worker id invalid");

        void *result_ptr = NULL;
        int join_result = pthread_join(worker, &result_ptr);
        assert(join_result == 0 && "test_join_loop_multiple_threads: join failed");
        assert((size_t)result_ptr == expected && "test_join_loop_multiple_threads: unexpected result");

        aggregate += (size_t)result_ptr;
        ++completed;
    }

    assert(completed == repetitions && "test_join_loop_multiple_threads: loop ended early");
    return aggregate;
}

// Launch many short-lived workers sequentially and verify their results.
int test_join_loop_multiple_threads(void)
{
    const size_t count = 2000;
    size_t total = run_worker_sequence(count);
    assert(total == 5 * count && "test_join_loop_multiple_threads: aggregate mismatch");
    return 0;
}
