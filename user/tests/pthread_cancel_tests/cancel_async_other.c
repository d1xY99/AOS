#include "../pthread_test_utils.h"
#include "assert.h"
#include "pthread.h"
#include "stdio.h"

static volatile int worker_started = 0;

static void *async_worker(void *arg) {
  (void)arg;
  int rv = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  assert(rv == 0);

  worker_started = 1;

  while (1) {
    test_brief_pause();
  }

  return (void *)0;
}

int cancel_async_other() {
  printf("Starting cancel_async_other...\n");

  pthread_t thread;
  worker_started = 0;

  int rv = pthread_create(&thread, NULL, async_worker, NULL);
  assert(rv == 0);

  test_wait_for_flag(&worker_started);

  rv = pthread_cancel(thread);
  assert(rv == 0);

  void *result = (void *)0;
  rv = pthread_join(thread, &result);
  assert(rv == 0);
  assert(result == TEST_PTHREAD_CANCELED);

  return 0;
}
