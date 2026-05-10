#include "assert.h"
#include "pthread.h"
#include "sched.h"

struct thread_arg
{
  int index;
  volatile int processed;
};

static void *echo_index(void *param)
{
  struct thread_arg *arg = (struct thread_arg *)param;
  assert(arg);
  arg->processed = arg->index * 2;
  sched_yield();
  return (void *)(size_t)arg->index;
}

int create_argument_roundtrip(void)
{
  const int thread_count = 12;
  pthread_t ids[thread_count];
  struct thread_arg args[thread_count];

  for (int i = 0; i < thread_count; ++i)
  {
    args[i].index = i + 1;
    args[i].processed = 0;
    int rv = pthread_create(&ids[i], NULL, echo_index, &args[i]);
    assert(rv == 0);
  }

  for (int i = 0; i < thread_count; ++i)
  {
    void *result = NULL;
    int rv = pthread_join(ids[i], &result);
    assert(rv == 0);
    assert(result == (void *)(size_t)(i + 1));
    assert(args[i].processed == (i + 1) * 2);
  }

  return 0;
}
