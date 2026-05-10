#include "pthread.h"
#include "stdio.h"
#include "assert.h"
#include <unistd.h>

static void *finished_worker(void *ignored)
{
    (void)ignored;
    return (void *)6;
}

static pthread_t spawn_finished_worker(void)
{
    pthread_t tid = 0;
    int rv = pthread_create(&tid, NULL, finished_worker, NULL);
    assert(rv == 0 && "test_join_completed_worker_returns_value: create worker failed");
    assert(tid != 0 && "test_join_completed_worker_returns_value: unexpected thread id");
    return tid;
}

static void ensure_second_join_fails(pthread_t target)
{
    void *marker = (void *)0x1234;
    int rv = pthread_join(target, &marker);
    assert(rv != 0 && "test_join_completed_worker_returns_value: second join must fail");
    assert(marker == (void *)0x1234 && "test_join_completed_worker_returns_value: marker should remain untouched");
}

// Wait for a thread that already ended and collect its value.
int test_join_completed_worker_returns_value(void)
{
    pthread_t worker = spawn_finished_worker();

    sleep(1);

    void *result = (void *)0;
    int rv = pthread_join(worker, &result);
    assert(rv == 0 && "test_join_completed_worker_returns_value: join should succeed");
    assert((size_t)result == 6 && "test_join_completed_worker_returns_value: wrong return payload");

    assert(pthread_self() != worker && "test_join_completed_worker_returns_value: worker should not be caller");

    ensure_second_join_fails(worker);

    return 0;
}
