#include <assert.h>
#include <stdio.h>
#include <time.h>

int clock_progress_test(void)
{
  printf("[clock_progress_test] verifying clock() increases during work\n");

  clock_t start = clock();
  volatile size_t sink = 0;
  const size_t iterations = 5 * 1000 * 1000;
  for (size_t i = 0; i < iterations; ++i)
    sink += i;

  clock_t end = clock();
  (void)sink;

  if (end < start)
  {
    printf("[clock_progress_test] ERROR: clock() moved backwards (start=%ld end=%ld)\n",
           (long)start, (long)end);
    return 1;
  }

  clock_t delta = end - start;
  printf("[clock_progress_test] delta ticks=%ld seconds=%.6f\n", (long)delta,
         (double)delta / CLOCKS_PER_SEC);
  assert(delta >= 0);

  if (delta == 0)
  {
    printf("[clock_progress_test] ERROR: no measurable clock progress\n");
    return 1;
  }

  return 0;
}
