#include <stdio.h>
#include <time.h>

int main(void) {
  printf("Starting clock_simple.c test...\n");
  clock_t start, end;
  start = clock();
  printf("Start time is %.5f seconds\n", (double)start / CLOCKS_PER_SEC);
  double cpu_time_used;
  size_t sum = 0;
  size_t limit = 1000000000;

  for (size_t i = 0; i < limit; i++) {
    sum += 1;
  }

  end = clock();

  printf("End time is %.5f seconds\n", (double)end / CLOCKS_PER_SEC);

  cpu_time_used = (double)(end - start) / CLOCKS_PER_SEC;

  printf("Sum: %zu\n", sum);
  printf("Time taken: %.5f seconds\n", cpu_time_used);

  return 0;
}
