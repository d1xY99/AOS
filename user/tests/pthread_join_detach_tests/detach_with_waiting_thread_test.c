#include "pthread.h"
#include "assert.h"
#include "stdio.h"
#include <unistd.h>
// #include <stdint.h>

static void *looping_worker(void *unused)
{
       (void)unused;
       while (1)
       {
              sleep(1);
       }
       return NULL;
}

static void *quick_finish(void *unused)
{
       (void)unused;
       return NULL;
}

static void *join_target(void *tid_arg)
{
       pthread_t target = (pthread_t)tid_arg;
       size_t join_result = pthread_join(target, NULL);
       assert(join_result != 0 &&
              "test_detach_with_waiting_thread: join should fail once target is detached");
       return (void *)join_result;
}

// Cancel a thread while another thread waits on join, then ensure the waiter can finish.
int test_detach_with_waiting_thread(void)
{
       pthread_t long_runner = 0;
       assert(pthread_create(&long_runner, NULL, looping_worker, NULL) == 0 &&
              "test_detach_with_waiting_thread: long running thread creation failed");
       assert(long_runner != 0 && "test_detach_with_waiting_thread: long runner id invalid");

       sleep(1);

       pthread_t waiting_joiner = 0;
       assert(pthread_create(&waiting_joiner, NULL, join_target, (void *)long_runner) == 0 &&
              "test_detach_with_waiting_thread: joiner creation failed");
       assert(waiting_joiner != 0 && "test_detach_with_waiting_thread: waiting joiner id invalid");

       sleep(1);

       assert(pthread_detach(long_runner) == 0 &&
              "test_detach_with_waiting_thread: detaching long runner failed");

       void *joiner_value = NULL;
       assert(pthread_join(waiting_joiner, &joiner_value) == 0 &&
              "test_detach_with_waiting_thread: waiting joiner should finish cleanly");
       int joiner_code = (int)(size_t)joiner_value;
       assert(joiner_code != 0 &&
              "test_detach_with_waiting_thread: joiner should report join failure");

       assert(pthread_join(long_runner, NULL) != 0 &&
              "test_detach_with_waiting_thread: detached thread must not be joinable");

       assert(pthread_cancel(long_runner) == 0 &&
              "test_detach_with_waiting_thread: cancel detached thread failed");
       sleep(1);

       void *sentinel = (void *)0x1010;
       int join_status = pthread_join(waiting_joiner, &sentinel);
       assert(join_status != 0 && "test_detach_with_waiting_thread: joiner already collected must fail");
       assert(sentinel == (void *)0x1010 && "test_detach_with_waiting_thread: sentinel should remain unchanged");

       pthread_t cleanup_worker = 0;
       assert(pthread_create(&cleanup_worker, NULL, quick_finish, NULL) == 0 &&
              "test_detach_with_waiting_thread: cleanup worker creation failed");
       assert(cleanup_worker != 0 && "test_detach_with_waiting_thread: cleanup worker id invalid");
       assert(pthread_join(cleanup_worker, NULL) == 0 &&
              "test_detach_with_waiting_thread: cleanup worker join should succeed");

       return 0;
}
