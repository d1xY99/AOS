#include "pthread.h"
#include "stdio.h"
#include "assert.h"
#include "sched.h"

static volatile int void_worker_executed = 0;

static void *void_worker(void *unused)
{
    (void)unused;
    void_worker_executed = 1;
    return NULL;
}

static void wait_until_worker_runs(void)
{
    while (!void_worker_executed)
        sched_yield();
}

// Joining a void worker should succeed even if no result is expected.
int test_join_void_worker(void)
{
    pthread_t helper = 0;
    int create_status = pthread_create(&helper, NULL, void_worker, NULL);
    assert(create_status == 0 && "test_join_void_worker: helper creation failed");
    assert(helper != 0 && "test_join_void_worker: helper id invalid");

    wait_until_worker_runs();

    int join_status = pthread_join(helper, NULL);
    assert(join_status == 0 && "test_join_void_worker: join without result failed");
    assert(void_worker_executed == 1 && "test_join_void_worker: worker flag should remain set");

    int second_join = pthread_join(helper, NULL);
    assert(second_join != 0 && "test_join_void_worker: cannot join collected helper again");

    void_worker_executed = 0;
    pthread_t second_helper = 0;
    create_status = pthread_create(&second_helper, NULL, void_worker, NULL);
    assert(create_status == 0 && "test_join_void_worker: second helper creation failed");
    assert(second_helper != 0 && "test_join_void_worker: second helper id invalid");
    wait_until_worker_runs();
    join_status = pthread_join(second_helper, NULL);
    assert(join_status == 0 && "test_join_void_worker: second join should succeed");

    void *sentinel = (void *)0x1234;
    int third_join = pthread_join(second_helper, &sentinel);
    assert(third_join != 0 && "test_join_void_worker: third join must fail");
    assert(sentinel == (void *)0x1234 && "test_join_void_worker: sentinel should stay unchanged");

    return 0;
}
