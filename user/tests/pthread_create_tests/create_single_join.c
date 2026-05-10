#include "assert.h"
#include "pthread.h"

static void *return_value(void *unused)
{
  (void)unused;
  return (void *)0xBEE5;
}

int create_single_join(void)
{
  pthread_t thread;
  int rv = pthread_create(&thread, NULL, return_value, NULL);
  assert(rv == 0);

  void *result = NULL;
  rv = pthread_join(thread, &result);
  assert(rv == 0);
  assert(result == (void *)0xBEE5);

  return 0;
}
