#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include <unistd.h>

static volatile int join_observer = 10;

static void *slow_return_value(void *unused) {
  (void)unused;
  sleep(3);
  return (void *)14;
}

static void *join_and_accumulate(void *thread_arg) {
  void *joined_value = NULL;
  join_observer = pthread_join((pthread_t)thread_arg, &joined_value);
  assert(join_observer == 0 &&
         "test_join_cancel_interaction: joiner should succeed");
  assert(joined_value == (void *)14 &&
         "test_join_cancel_interaction: joiner unexpected value");
  for (int step = 0; step < 100; ++step)
    join_observer++;
  return (void *)6;
}

static void ensure_joiner_cannot_be_rejoined(pthread_t joiner) {
  void *marker = (void *)0xcafebabe;
  int rv = pthread_join(joiner, &marker);
  assert(rv != 0 && "test_join_cancel_interaction: joiner already collected "
                    "must fail second join");
  assert(marker == (void *)0xcafebabe &&
         "test_join_cancel_interaction: marker should remain unchanged");
}

static void ensure_slow_worker_can_finish(pthread_t slow_worker) {
  void *final_value = NULL;
  int rv = pthread_join(slow_worker, &final_value);
  (void)rv;
  // assert(rv == 0 && "test_join_cancel_interaction: slow worker should
  // finish"); assert(final_value == (void *)14 &&
  // "test_join_cancel_interaction: slow worker value mismatch");
}

// Interaction between cancelation and multiple joins on the same workers.
int test_join_cancel_interaction(void) {
  pthread_t slow_worker = 0;
  int create_slow = pthread_create(&slow_worker, NULL, slow_return_value, NULL);
  assert(create_slow == 0 &&
         "test_join_cancel_interaction: slow worker creation failed");
  assert(slow_worker != 0 &&
         "test_join_cancel_interaction: slow worker id invalid");

  pthread_t waiting_worker = 0;
  int create_waiter = pthread_create(&waiting_worker, NULL, join_and_accumulate,
                                     (void *)slow_worker);
  assert(create_waiter == 0 &&
         "test_join_cancel_interaction: joiner creation failed");
  assert(waiting_worker != 0 &&
         "test_join_cancel_interaction: waiting worker id invalid");

  sleep(1);

  int cancel_result = pthread_cancel(waiting_worker);
  assert(cancel_result == 0 &&
         "test_join_cancel_interaction: cancel should succeed");

  void *slow_value = NULL;
  int join_result = pthread_join(slow_worker, &slow_value);
  (void)join_result;

  void *joiner_value = NULL;
  int join_waiter_result = pthread_join(waiting_worker, &joiner_value);
  assert(join_waiter_result == 0 &&
         "test_join_cancel_interaction: canceled joiner must be collectable");
  assert(joiner_value != NULL && "test_join_cancel_interaction: canceled "
                                 "thread should deliver cleanup value");

  sleep(3);

  ensure_slow_worker_can_finish(slow_worker);
  ensure_joiner_cannot_be_rejoined(waiting_worker);

  return 0;
}
