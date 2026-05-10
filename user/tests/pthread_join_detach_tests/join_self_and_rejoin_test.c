#include <stdio.h>
#include <pthread.h>
#include <assert.h>

static void *noop_worker(void *arg)
{
  (void)arg;
  return NULL;
}

static void ensure_join_failure_reuses_code(pthread_t victim)
{
  void *sentinel = (void *)0x12345678;
  int rv = pthread_join(victim, &sentinel);
  assert(rv == 3 && "test_join_self_and_rejoin: repeated join should return ESRCH");
  assert(sentinel == (void *)0x12345678 && "test_join_self_and_rejoin: sentinel should remain unchanged");
}

// Joining yourself must fail and joining a finished thread twice must error.
int test_join_self_and_rejoin(void)
{
  pthread_t worker = 0;
  int create_status = pthread_create(&worker, NULL, noop_worker, NULL);
  assert(create_status == 0 && "test_join_self_and_rejoin: worker creation failed");
  assert(worker != 0 && "test_join_self_and_rejoin: worker id invalid");

  int self_join = pthread_join(pthread_self(), NULL);
  assert(self_join == 35 && "test_join_self_and_rejoin: joining self should return EDEADLK");

  int first_join = pthread_join(worker, NULL);
  assert(first_join == 0 && "test_join_self_and_rejoin: first join should succeed");
  int second_join = pthread_join(worker, NULL);
  assert(second_join == 3 && "test_join_self_and_rejoin: thread already collected should return ESRCH");

  ensure_join_failure_reuses_code(worker);

  pthread_t fresh_worker = 0;
  create_status = pthread_create(&fresh_worker, NULL, noop_worker, NULL);
  assert(create_status == 0 && "test_join_self_and_rejoin: fresh worker creation failed");
  assert(fresh_worker != 0 && "test_join_self_and_rejoin: fresh worker id invalid");
  void *fresh_value = (void *)0xbead;
  int fresh_join = pthread_join(fresh_worker, &fresh_value);
  assert(fresh_join == 0 && "test_join_self_and_rejoin: fresh worker join should succeed");
  assert(fresh_value == NULL && "test_join_self_and_rejoin: noop worker should return NULL");

  return 0;
}
