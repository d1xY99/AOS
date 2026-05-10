#include "pthread.h"
#include "assert.h"
#include <unistd.h>

static void *sleep_and_return(void *unused)
{
    (void)unused;
    sleep(1);
    return (void *)0x55;
}

int test_detach_self_twice(void)
{
    pthread_t self_thread = pthread_self();
    int status = pthread_detach(self_thread);
    assert(status == 0 && "test_detach_self_twice: detaching self should succeed first time");

    status = pthread_detach(self_thread);
    assert(status != 0 && "test_detach_self_twice: detaching self twice must fail");

    pthread_t worker = 0;
    assert(pthread_create(&worker, NULL, sleep_and_return, NULL) == 0 &&
           "test_detach_self_twice: worker creation failed");
    assert(worker != 0 && "test_detach_self_twice: worker id invalid");

    assert(pthread_detach(worker) == 0 &&
           "test_detach_self_twice: detaching worker should succeed");

    void *sentinel = (void *)0x99;
    int join_status = pthread_join(worker, &sentinel);
    assert(join_status != 0 && "test_detach_self_twice: detached worker must not be joinable");
    assert(sentinel == (void *)0x99 && "test_detach_self_twice: sentinel should remain unchanged");

    return 0;
}
