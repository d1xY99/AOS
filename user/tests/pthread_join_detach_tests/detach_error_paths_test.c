#include "stdio.h"
#include "pthread.h"
#include "assert.h"
#include <unistd.h>

static void *sleep_briefly(void *unused)
{
  (void)unused;
  sleep(1);
  return NULL;
}

static pthread_t spawn_sleep_worker(void)
{
  pthread_t tid = 0;
  int create_status = pthread_create(&tid, NULL, sleep_briefly, NULL);
  assert(create_status == 0 && "test_detach_error_paths: worker creation failed");
  assert(tid != 0 && "test_detach_error_paths: worker id invalid");
  return tid;
}

// Basic detach behaviour: detach invalid id, detach once, join should fail, second detach fails.
int test_detach_error_paths(void)
{
  int detach_status = pthread_detach((pthread_t)29387);
  assert(detach_status != 0 && "test_detach_error_paths: detaching random id must fail");

  pthread_t first_worker = spawn_sleep_worker();

  detach_status = pthread_detach(first_worker);
  assert(detach_status == 0 && "test_detach_error_paths: detach should succeed once");

  int join_status = pthread_join(first_worker, NULL);
  assert(join_status != 0 && "test_detach_error_paths: detached thread must not be joinable");
  join_status = pthread_join((pthread_t)29387, NULL);
  assert(join_status != 0 && "test_detach_error_paths: joining random id must fail");

  pthread_t second_worker = spawn_sleep_worker();

  detach_status = pthread_detach(second_worker);
  assert(detach_status == 0 && "test_detach_error_paths: initial detach should succeed");

  detach_status = pthread_detach(second_worker);
  assert(detach_status != 0 && "test_detach_error_paths: repeated detach must fail");

  join_status = pthread_join(second_worker, NULL);
  assert(join_status != 0 && "test_detach_error_paths: detached second worker must not be joinable");

  pthread_t third_worker = spawn_sleep_worker();
  join_status = pthread_join(third_worker, NULL);
  assert(join_status == 0 && "test_detach_error_paths: third worker should join successfully when not detached");

  return 0;
}
