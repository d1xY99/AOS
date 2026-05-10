#include "assert.h"
#include "pthread.h"

static void *noop(void *unused)
{
  (void)unused;
  return (void *)0;
}

int create_invalid_inputs(void)
{
  pthread_t thread;
  int rv;

  rv = pthread_create(NULL, NULL, noop, NULL);
  assert(rv != 0);

  rv = pthread_create(&thread, NULL, NULL, NULL);
  assert(rv != 0);

  //! don't know if this is a valid test case
  // rv = pthread_create((pthread_t *)0xC0DE0000UL, NULL, noop, NULL);
  // assert(rv != 0);

  // rv = pthread_create(&thread, NULL, (void *(*)(void *))0x7FFFFFFFUL, NULL);
  // assert(rv != 0);

  return 0;
}
