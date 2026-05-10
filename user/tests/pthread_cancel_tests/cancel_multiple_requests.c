#include "../pthread_test_utils.h"
#include "assert.h"
#include "pthread.h"
#include <stdio.h>

static volatile int worker_ready = 0;

static void *multi_cancel_worker(void *arg)
{
  (void)arg;
  int rv = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  assert(rv == 0);
  worker_ready = 1;

  while (1)
    test_brief_pause();

  return (void *)0;
}

int cancel_multiple_requests()
{
  printf("Starting cancel_multiple_requests test...\n");

  pthread_t thread;
  worker_ready = 0;

  int rv = pthread_create(&thread, NULL, multi_cancel_worker, NULL);
  assert(rv == 0);

  test_wait_for_flag(&worker_ready);

  rv = pthread_cancel(thread);
  assert(rv == 0);

  rv = pthread_cancel(thread);
  assert(rv == 0 && "Repeated cancellation should still report success");

  void *result = NULL;
  rv = pthread_join(thread, &result);
  assert(rv == 0);
  assert(result == TEST_PTHREAD_CANCELED);

  printf("cancel_multiple_requests: thread canceled despite duplicate requests\n");
  return 0;
}
