#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int usleep_return_test(void)
{
  const useconds_t intervals[] = {100000, 200000, 500000}; // up to 0.5s

  clock_t last = clock();
  for (unsigned i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i)
  {
    useconds_t us = intervals[i];
    printf("[usleep_return_test] sleeping for %u microseconds\n", (unsigned)us);
    int rv = usleep(us);
    if (rv != 0)
    {
      printf("[usleep_return_test] ERROR: usleep(%u) returned %d\n", (unsigned)us, rv);
      return 1;
    }

    clock_t now = clock();
    if (now < last)
    {
      printf("[usleep_return_test] ERROR: clock decreased (prev=%ld now=%ld)\n",
             (long)last, (long)now);
      return 1;
    }
    last = now;
  }

  printf("[usleep_return_test] usleep() handled all intervals\n");
  return 0;
}
