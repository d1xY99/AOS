#include "../pthread_test_utils.h"
#include "assert.h"
#include "pthread.h"
#include <stdio.h>
#include <unistd.h>

static void *self_cancel_worker(void *arg)
{
    (void)arg;
    int rv = pthread_cancel(pthread_self());
    assert(rv == 0);

    assert(0 && "self-cancelled thread should not continue executing");
    return (void *)0;
}

// static void *deferred_self_cancel(void *arg)
// {
//     (void)arg;
//     int rv = pthread_cancel(pthread_self());
//     assert(rv == 0);

//     assert(0 && "deferred cancel should terminate");
//     return (void *)0;
// }

static void *cancel_then_work(void *arg)
{
    (void)arg;
    /* Simulate some work before canceling self */
    for (int i = 0; i < 10; i++)
    {
        volatile int x = i;
        (void)x;
    }

    int rv = pthread_cancel(pthread_self());
    assert(rv == 0);

    assert(0 && "should not reach here after self-cancel");
    return (void *)0;
}

static void *multi_cancel_attempt(void *arg)
{
    (void)arg;
    int rv = pthread_cancel(pthread_self());
    assert(rv == 0);

    assert(0 && "first self-cancel should terminate thread");
    return (void *)0;
}

int cancel_self_immediate_fixed()
{
    printf("Starting cancel_self_immediate test...\n");

    printf("Step 1: Basic self-cancel\n");
    pthread_t thread;
    int rv = pthread_create(&thread, NULL, self_cancel_worker, NULL);
    assert(rv == 0 && "thread creation failed");

    void *result = (void *)0;
    rv = pthread_join(thread, &result);
    assert(rv == 0 && "join should succeed");
    assert(result == TEST_PTHREAD_CANCELED && "thread should be canceled");

    printf("Step 2: Self-cancel after doing work\n");
    pthread_t worker = 0;
    rv = pthread_create(&worker, NULL, cancel_then_work, NULL);
    assert(rv == 0 && "thread creation failed");

    result = (void *)0;
    rv = pthread_join(worker, &result);
    assert(rv == 0 && "join should succeed");
    assert(result == TEST_PTHREAD_CANCELED && "thread should be canceled");

    printf("Step 3: Multiple threads self-canceling\n");
    pthread_t threads[3];
    for (int i = 0; i < 3; i++)
    {
        rv = pthread_create(&threads[i], NULL, cancel_then_work, NULL);
        assert(rv == 0 && "thread creation failed");
    }

    for (int i = 0; i < 3; i++)
    {
        result = (void *)0;
        rv = pthread_join(threads[i], &result);
        assert(rv == 0 && "join should succeed");
        assert(result == TEST_PTHREAD_CANCELED && "thread should be canceled");
    }

    printf("Step 4: Rapid self-cancel (first one terminates)\n");
    pthread_t multi = 0;
    rv = pthread_create(&multi, NULL, multi_cancel_attempt, NULL);
    assert(rv == 0 && "thread creation failed");

    result = (void *)0;
    rv = pthread_join(multi, &result);
    assert(rv == 0 && "join should succeed");
    assert(result == TEST_PTHREAD_CANCELED && "thread should be canceled");

    printf("All steps completed successfully\n");
    return 0;
}
