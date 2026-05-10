
#include <stdio.h>
#include <time.h>

int main(void) {
  printf("Starting clock_simple_sleep.c test...\n");
  clock_t start, end;
  double cpu_time_used;

  start = clock();

  sleep(2);

  end = clock();

  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

  printf("Time taken: %.5f seconds\n", cpu_time_used);

  return 0;
}
