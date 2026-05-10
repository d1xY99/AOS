#include "pthread.h"
#include "assert.h"
#include "stdio.h"
#include <unistd.h>

static void *infinite_sleep(void *unused)
{
  (void)unused;
  while (1)
  {
    sleep(1);
  }
  return NULL;
}

static void *instant_return(void *unused)
{
  (void)unused;
  return NULL;
}

// Detaching a thread should make future joins fail, whether or not the worker already exited.
int test_detach_not_joinable(void)
{
  pthread_t live_thread = 0;
  assert(pthread_create(&live_thread, NULL, infinite_sleep, NULL) == 0 &&
         "test_detach_not_joinable: live thread creation failed");
  assert(live_thread != 0 && "test_detach_not_joinable: live thread id invalid");
  assert(pthread_detach(live_thread) == 0 &&
         "test_detach_not_joinable: detaching live thread failed");
  assert(pthread_join(live_thread, NULL) != 0 &&
         "test_detach_not_joinable: detached live thread must not be joinable");
  assert(pthread_detach(live_thread) != 0 &&
         "test_detach_not_joinable: detaching live thread a second time must fail");

  pthread_t finished_thread = 0;
  assert(pthread_create(&finished_thread, NULL, instant_return, NULL) == 0 &&
         "test_detach_not_joinable: finished thread creation failed");
  assert(finished_thread != 0 && "test_detach_not_joinable: finished thread id invalid");
  sleep(1);
  assert(pthread_detach(finished_thread) == 0 &&
         "test_detach_not_joinable: detaching finished thread failed");
  void *sentinel = (void *)0x7777;
  assert(pthread_join(finished_thread, &sentinel) != 0 &&
         "test_detach_not_joinable: detached finished thread must not be joinable");
  assert(sentinel == (void *)0x7777 &&
         "test_detach_not_joinable: failed join must not modify sentinel");
  assert(pthread_detach(finished_thread) != 0 &&
         "test_detach_not_joinable: detaching finished thread twice must fail");

  return 0;
}
