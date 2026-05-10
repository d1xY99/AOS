#include "pthread.h"
#include "assert.h"
#include "stdio.h"
#include "stdlib.h"

static void *return_null(void *unused)
{
    (void)unused;
    return NULL;
}

static void *return_value(void *arg)
{
    return arg;
}

static void *thread_with_work(void *unused)
{
    (void)unused;
    for (int i = 0; i < 100; i++)
    {
        volatile int x = i * 2;
        (void)x;
    }
    return NULL;
}

int test_join_returns_null_pointer(void)
{
    printf("Step 1: Test basic null return from thread\n");
    pthread_t worker = 0;
    int create_status = pthread_create(&worker, NULL, return_null, NULL);
    assert(create_status == 0 && "test_join_returns_null_pointer: worker creation failed");
    assert(worker != 0 && "test_join_returns_null_pointer: worker id invalid");

    void *result = (void *)0x9999;
    int join_status = pthread_join(worker, &result);
    assert(join_status == 0 && "test_join_returns_null_pointer: join should succeed");
    assert(result == NULL && "test_join_returns_null_pointer: expected NULL result");

    printf("Step 2: Verify double join fails\n");
    join_status = pthread_join(worker, &result);
    assert(join_status != 0 && "test_join_returns_null_pointer: second join should fail");
    assert(result == NULL && "test_join_returns_null_pointer: result should stay NULL after failure");

    printf("Step 3: Test multiple threads with null returns\n");
    pthread_t workers[3];
    for (int i = 0; i < 3; i++)
    {
        create_status = pthread_create(&workers[i], NULL, return_null, NULL);
        assert(create_status == 0 && "failed to create worker thread");
    }
    for (int i = 0; i < 3; i++)
    {
        result = (void *)0xDEADBEEF;
        join_status = pthread_join(workers[i], &result);
        assert(join_status == 0 && "join should succeed");
        assert(result == NULL && "all workers should return NULL");
    }

    printf("Step 4: Test thread that does work then returns null\n");
    pthread_t busy = 0;
    create_status = pthread_create(&busy, NULL, thread_with_work, NULL);
    assert(create_status == 0 && "failed to create busy thread");
    result = (void *)0xBEEF;
    join_status = pthread_join(busy, &result);
    assert(join_status == 0 && "join busy thread should succeed");
    assert(result == NULL && "busy thread should return NULL");

    printf("Step 5: Test thread returning non-null value\n");
    pthread_t value_thread = 0;
    void *test_value = (void *)0x12345678;
    create_status = pthread_create(&value_thread, NULL, return_value, test_value);
    assert(create_status == 0 && "failed to create value thread");
    result = NULL;
    join_status = pthread_join(value_thread, &result);
    assert(join_status == 0 && "join value thread should succeed");
    assert(result == test_value && "thread should return the passed value");

    printf("All steps completed successfully\n");
    return 0;
}
