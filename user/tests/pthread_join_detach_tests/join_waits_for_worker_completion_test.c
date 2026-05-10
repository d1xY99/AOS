#include "stdio.h"
#include "pthread.h"
#include "assert.h"
#include <unistd.h>


static void *delayed_five(void *unused)
{
    (void)unused;
    sleep(1);
    return (void *)5;
}

static void verify_second_join_failure(pthread_t target)
{
    void *sentinel = (void *)0xfeedface;
    int rv = pthread_join(target, &sentinel);
    assert(rv != 0 && "test_join_waits_with_value_ptr: second join should fail");
    assert(sentinel == (void *)0xfeedface && "test_join_waits_with_value_ptr: sentinel must remain unchanged");
}

// Join must block until the worker has finished and return its result.
int test_join_waits_with_value_ptr(void)
{
    pthread_t worker = 0;
    int create_status = pthread_create(&worker, NULL, delayed_five, NULL);
    assert(create_status == 0 && "test_join_waits_with_value_ptr: worker creation failed");
    assert(worker != 0 && "test_join_waits_with_value_ptr: worker id invalid");

    void *result = NULL;
    int join_status = pthread_join(worker, &result);
    assert(join_status == 0 && "test_join_waits_with_value_ptr: join failed");
    assert((size_t)result == 5 && "test_join_waits_with_value_ptr: result mismatch");

    verify_second_join_failure(worker);

    return 0;
}
