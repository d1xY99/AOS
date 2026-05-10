#include "../pthread_test_utils.h"
#include "assert.h"
#include "pthread.h"
#include <stdio.h>

static volatile int worker_ready = 0;
static volatile int allow_exit = 0;
static void *const WORKER_RESULT = (void *)0x1234;

static void *disable_cancel_worker(void *arg)
{
  (void)arg;
  int rv = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  assert(rv == 0);

  rv = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  assert(rv == 0);

  worker_ready = 1;
  test_wait_for_flag(&allow_exit);

  return WORKER_RESULT;
}

int cancel_disable_ignored()
{
  printf("Starting cancel_disable_ignored test...\n");

  pthread_t thread;
  worker_ready = 0;
  allow_exit = 0;

  int rv = pthread_create(&thread, NULL, disable_cancel_worker, NULL);
  assert(rv == 0);

  test_wait_for_flag(&worker_ready);

  rv = pthread_cancel(thread);
  assert(rv == 0);

  allow_exit = 1;

  void *result = NULL;
  rv = pthread_join(thread, &result);
  assert(rv == 0);
  assert(result == WORKER_RESULT &&
         "Thread with cancellation disabled should exit normally");

  printf("cancel_disable_ignored: cancel request remained pending and thread exited normally\n");
  return 0;
}
