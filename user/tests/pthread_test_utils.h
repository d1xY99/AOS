#pragma once

#include "sched.h"

#ifndef PTHREAD_CANCEL_ENABLE
#define PTHREAD_CANCEL_ENABLE 0
#endif

#ifndef PTHREAD_CANCEL_DISABLE
#define PTHREAD_CANCEL_DISABLE 1
#endif

#ifndef PTHREAD_CANCEL_DEFERRED
#define PTHREAD_CANCEL_DEFERRED 0
#endif

#ifndef PTHREAD_CANCEL_ASYNCHRONOUS
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#endif

#ifndef TEST_PTHREAD_CANCELED
#define TEST_PTHREAD_CANCELED ((void *)-1)
#endif

#ifndef TEST_ESRCH
#define TEST_ESRCH 3
#endif

#ifndef TEST_EINVAL
#define TEST_EINVAL 22
#endif

#ifndef TEST_EDEADLK
#define TEST_EDEADLK 35
#endif

static inline void test_wait_for_flag(volatile int *flag)
{
  while (!*flag)
  {
    sched_yield();
  }
}

static inline void test_brief_pause(void)
{
  for (volatile int i = 0; i < 1000; ++i)
  {
    // simple delay to reduce busy spinning
  }
  sched_yield();
}
