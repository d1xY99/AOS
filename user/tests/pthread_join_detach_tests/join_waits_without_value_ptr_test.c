#include "pthread.h"
#include "assert.h"
#include "stdio.h"
#include <unistd.h>

static void *delayed_no_result(void *unused)
{
    (void)unused;
    sleep(1);
    return (void *)0x1; // should be ignored by caller
}

static void *immediate_return(void *unused)
{
    (void)unused;
    return (void *)0xDEADBEEF;
}

static void *thread_with_work(void *unused)
{
    (void)unused;
    for (int i = 0; i < 200; i++)
    {
        volatile int x = i * 2;
        (void)x;
    }
    return (void *)0x999;
}

static void *quick_null_return(void *unused)
{
    (void)unused;
    return NULL;
}

int test_join_waits_without_value_ptr(void)
{
    printf("Step 1: Basic join without value pointer\n");
    pthread_t worker = 0;
    int create_status = pthread_create(&worker, NULL, delayed_no_result, NULL);
    assert(create_status == 0 && "worker creation failed");
    assert(worker != 0 && "worker id invalid");

    int join_status = pthread_join(worker, NULL);
    assert(join_status == 0 && "join should succeed");

    printf("Step 2: Verify second join fails\n");
    join_status = pthread_join(worker, NULL);
    assert(join_status != 0 && "second join should fail");

    printf("Step 3: Multiple threads joining without value pointers\n");
    pthread_t workers[3];
    for (int i = 0; i < 3; i++)
    {
        create_status = pthread_create(&workers[i], NULL, immediate_return, NULL);
        assert(create_status == 0 && "thread creation failed");
    }

    for (int i = 0; i < 3; i++)
    {
        join_status = pthread_join(workers[i], NULL);
        assert(join_status == 0 && "join should succeed");
    }

    printf("Step 4: Verify all second joins fail\n");
    for (int i = 0; i < 3; i++)
    {
        join_status = pthread_join(workers[i], NULL);
        assert(join_status != 0 && "second join should fail");
    }

    printf("Step 5: Join busy thread without capturing return value\n");
    pthread_t busy = 0;
    create_status = pthread_create(&busy, NULL, thread_with_work, NULL);
    assert(create_status == 0 && "busy thread creation failed");

    join_status = pthread_join(busy, NULL);
    assert(join_status == 0 && "busy thread join should succeed");

    printf("Step 6: Verify busy thread second join fails\n");
    join_status = pthread_join(busy, NULL);
    assert(join_status != 0 && "second join should fail");

    printf("Step 7: Thread returning NULL joined without value pointer\n");
    pthread_t null_thread = 0;
    create_status = pthread_create(&null_thread, NULL, quick_null_return, NULL);
    assert(create_status == 0 && "null thread creation failed");

    join_status = pthread_join(null_thread, NULL);
    assert(join_status == 0 && "null thread join should succeed");

    printf("Step 8: Verify multiple sequential failures\n");
    for (int i = 0; i < 3; i++)
    {
        join_status = pthread_join(null_thread, NULL);
        assert(join_status != 0 && "repeated joins should always fail");
    }

    printf("All steps completed successfully\n");
    return 0;
}
