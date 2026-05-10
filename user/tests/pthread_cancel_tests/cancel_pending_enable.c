#include "../pthread_test_utils.h"
#include "assert.h"
#include "pthread.h"
#include <stdio.h>

static volatile int setup_complete = 0;
static volatile int cancel_release = 0;

static void *pending_cancel_worker(void *arg) {
  (void)arg;

  int rv = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  assert(rv == 0);

  rv = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
  assert(rv == 0);

  setup_complete = 1;

  test_wait_for_flag(&cancel_release);

  rv = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  assert(rv == 0);

  rv = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  assert(rv == 0);

  assert(0 && "cancel_pending_enable: pending cancel must terminate thread "
              "after enabling async");
  return (void *)0;
}

int cancel_pending_enable() {
  printf("Starting cancel_pending_enable test...\n");

  pthread_t thread;
  setup_complete = 0;
  cancel_release = 0;

  int rv = pthread_create(&thread, NULL, pending_cancel_worker, NULL);
  assert(rv == 0);

  test_wait_for_flag(&setup_complete);

  rv = pthread_cancel(thread);
  assert(rv == 0);

  cancel_release = 1;

  void *result = (void *)0;
  rv = pthread_join(thread, &result);
  assert(rv == 0);
  assert(result == TEST_PTHREAD_CANCELED);

  return 0;
}
