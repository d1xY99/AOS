#include "../pthread_test_utils.h"
#include "assert.h"
#include "pthread.h"
#include "stdio.h"
#include "unistd.h"

static void *instant_exit(void *arg)
{
  (void)arg;
  return (void *)0xBEEF;
}

// static void *delayed_thread(void *arg)
// {
//   (void)arg;
//   sleep(2);
//   return (void *)0xCAFE;
// }

static void *quick_work(void *arg)
{
  (void)arg;
  for (int i = 0; i < 50; i++)
  {
    volatile int x = i;
    (void)x;
  }
  return NULL;
}

int cancel_invalid_target()
{
  printf("Starting cancel_invalid_target test...\n");

  printf("Step 1: Cancel already exited thread\n");
  pthread_t finished_thread;
  int rv = pthread_create(&finished_thread, NULL, instant_exit, NULL);
  assert(rv == 0 && "thread creation failed");

  rv = pthread_join(finished_thread, NULL);
  assert(rv == 0 && "join should succeed");

  rv = pthread_cancel(finished_thread);
  assert(rv == TEST_ESRCH && "cancel on exited thread should fail");

  printf("Step 2: Cancel non-existent thread ID\n");
  rv = pthread_cancel(finished_thread + 1000);
  assert(rv == TEST_ESRCH && "cancel on invalid ID should fail");

  printf("Step 3: Cancel with invalid thread IDs\n");
  rv = pthread_cancel(0);
  assert(rv == TEST_ESRCH && "cancel on zero ID should fail");

  rv = pthread_cancel((pthread_t)0xDEADBEEF);
  assert(rv == TEST_ESRCH && "cancel on garbage ID should fail");

  printf("Step 4: Multiple threads - cancel after all exit\n");
  pthread_t threads[3];
  for (int i = 0; i < 3; i++)
  {
    rv = pthread_create(&threads[i], NULL, quick_work, NULL);
    assert(rv == 0 && "thread creation failed");
  }

  for (int i = 0; i < 3; i++)
  {
    rv = pthread_join(threads[i], NULL);
    assert(rv == 0 && "join should succeed");
  }

  for (int i = 0; i < 3; i++)
  {
    rv = pthread_cancel(threads[i]);
    assert(rv == TEST_ESRCH && "cancel on exited thread should fail");
  }

  printf("Step 5: Cancel with thread ID + various offsets\n");
  pthread_t ref_thread;
  rv = pthread_create(&ref_thread, NULL, instant_exit, NULL);
  assert(rv == 0 && "thread creation failed");

  rv = pthread_join(ref_thread, NULL);
  assert(rv == 0 && "join should succeed");

  rv = pthread_cancel(ref_thread + 1);
  assert(rv == TEST_ESRCH && "cancel on offset ID should fail");

  rv = pthread_cancel(ref_thread + 100);
  assert(rv == TEST_ESRCH && "cancel on large offset ID should fail");

  rv = pthread_cancel(ref_thread - 1);
  assert(rv == TEST_ESRCH && "cancel on negative offset ID should fail");

  printf("Step 6: Verify canceling same invalid ID multiple times\n");
  for (int i = 0; i < 5; i++)
  {
    rv = pthread_cancel(finished_thread);
    assert(rv == TEST_ESRCH && "repeated cancel should consistently fail");
  }

  printf("All steps completed successfully\n");
  return 0;
}
