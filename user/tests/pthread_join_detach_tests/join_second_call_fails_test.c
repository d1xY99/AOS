#include "pthread.h"
#include "assert.h"
#include "stdio.h"
#include <unistd.h>

static void *sleep_then_return_five(void *ignored)
{
    (void)ignored;
    sleep(1);
    return (void *)5;
}

static void *quick_return(void *arg)
{
    return arg;
}

static void *thread_no_value(void *ignored)
{
    (void)ignored;
    for (int i = 0; i < 50; i++)
    {
        volatile int x = i;
        (void)x;
    }
    return (void *)999;
}

static void confirm_pointer_unchanged(void *pointer_before, void *pointer_after)
{
    assert(pointer_before == pointer_after &&
           "sentinel should remain unchanged");
}

// Joining a thread twice should fail the second time and leave the pointer untouched.
int test_join_second_attempt_fails(void)
{
    printf("Step 1: Basic double join test\n");
    pthread_t worker = 0;
    int create_result = pthread_create(&worker, NULL, sleep_then_return_five, NULL);
    assert(create_result == 0 && "worker creation failed");
    assert(worker != 0 && "worker id must be valid");

    void *first_result = NULL;
    int join_status = pthread_join(worker, &first_result);
    assert(join_status == 0 && "first join should succeed");
    assert((size_t)first_result == 5 && "unexpected first result");

    void *sentinel = (void *)42;
    int second_join_status = pthread_join(worker, &sentinel);
    assert(second_join_status != 0 && "second join must fail");
    confirm_pointer_unchanged((void *)42, sentinel);

    printf("Step 2: Test multiple failed join attempts\n");
    int third_join_status = pthread_join(worker, NULL);
    assert(third_join_status != 0 && "repeated joins must continue to fail");

    void *another_sentinel = (void *)0xDEAD;
    int fourth_join = pthread_join(worker, &another_sentinel);
    assert(fourth_join != 0 && "join should still fail");
    confirm_pointer_unchanged((void *)0xDEAD, another_sentinel);

    printf("Step 3: Test different thread with quick completion\n");
    pthread_t quick = 0;
    create_result = pthread_create(&quick, NULL, quick_return, (void *)777);
    assert(create_result == 0 && "quick thread creation failed");

    void *quick_result = NULL;
    join_status = pthread_join(quick, &quick_result);
    assert(join_status == 0 && "first join should succeed");
    assert((size_t)quick_result == 777 && "should get correct value");

    sentinel = (void *)111;
    second_join_status = pthread_join(quick, &sentinel);
    assert(second_join_status != 0 && "second join must fail");
    confirm_pointer_unchanged((void *)111, sentinel);

    printf("Step 4: Test with multiple threads\n");
    pthread_t workers[3];
    for (int i = 0; i < 3; i++)
    {
        create_result = pthread_create(&workers[i], NULL, thread_no_value, NULL);
        assert(create_result == 0 && "thread creation failed");
    }

    for (int i = 0; i < 3; i++)
    {
        void *res = NULL;
        join_status = pthread_join(workers[i], &res);
        assert(join_status == 0 && "first join should succeed");
        assert((size_t)res == 999 && "should get correct return value");

        sentinel = (void *)0xCAFE;
        second_join_status = pthread_join(workers[i], &sentinel);
        assert(second_join_status != 0 && "second join must fail");
        confirm_pointer_unchanged((void *)0xCAFE, sentinel);
    }

    printf("Step 5: Verify double join with NULL output pointer\n");
    pthread_t final = 0;
    create_result = pthread_create(&final, NULL, quick_return, (void *)555);
    assert(create_result == 0 && "final thread creation failed");

    join_status = pthread_join(final, NULL);
    assert(join_status == 0 && "first join with NULL should succeed");

    second_join_status = pthread_join(final, NULL);
    assert(second_join_status != 0 && "second join with NULL must still fail");

    printf("All steps completed successfully\n");
    return 0;
}
