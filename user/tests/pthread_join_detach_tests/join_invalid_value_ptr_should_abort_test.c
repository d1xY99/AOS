#include "stdio.h"
#include "pthread.h"
#include "assert.h"
#include "unistd.h"

static void *return_zero(void *unused)
{
  (void)unused;
  return NULL;
}

static void *return_value(void *arg)
{
  return arg;
}

static void *busy_thread(void *unused)
{
  (void)unused;
  for (int i = 0; i < 100; i++)
  {
    volatile int x = i;
    (void)x;
  }
  return (void *)42;
}

// Passing a kernel address as value_ptr should result in a fatal error.
int test_join_invalid_value_ptr_should_abort(void)
{
  printf("Step 1: Test valid join with stack address\n");
  pthread_t safe_thread;
  int create_status = pthread_create(&safe_thread, NULL, return_zero, NULL);
  assert(create_status == 0 && "safe worker creation failed");
  assert(safe_thread != 0 && "safe worker id invalid");

  void *safe_value = (void *)0xabad1dea;
  int join_status = pthread_join(safe_thread, &safe_value);
  assert(join_status == 0 && "safe join should succeed");
  assert(safe_value == NULL && "safe worker should return NULL");

  printf("Step 2: Test valid join with multiple threads\n");
  pthread_t workers[2];
  for (int i = 0; i < 2; i++)
  {
    create_status = pthread_create(&workers[i], NULL, return_value, (void *)(long)(i + 10));
    assert(create_status == 0 && "worker creation failed");
  }

  for (int i = 0; i < 2; i++)
  {
    void *result = NULL;
    join_status = pthread_join(workers[i], &result);
    assert(join_status == 0 && "join should succeed");
    assert((long)result == i + 10 && "should get correct return value");
  }

  printf("Step 3: Test valid join with busy thread\n");
  pthread_t busy = 0;
  create_status = pthread_create(&busy, NULL, busy_thread, NULL);
  assert(create_status == 0 && "busy thread creation failed");

  void *busy_result = NULL;
  join_status = pthread_join(busy, &busy_result);
  assert(join_status == 0 && "busy thread join should succeed");
  assert((long)busy_result == 42 && "should get correct value from busy thread");

  printf("Step 4: Attempting invalid kernel address join (should abort)\n");
  pthread_t crash_thread;
  create_status = pthread_create(&crash_thread, NULL, return_zero, NULL);
  assert(create_status == 0 && "crash worker creation failed");
  assert(crash_thread != 0 && "crash worker id invalid");

  // This should trigger a fault/abort due to kernel address
  pthread_join(crash_thread, (void *)0x7ffffffa0000);
  assert(0 && "join should have crashed before this point");

  return 0;
}
