#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int brkDecrease() {
  void *orig = sbrk(0);

  void *higher = (void *)((size_t)orig + 2 * PAGE_SIZE);

  if (brk(higher) == -1) {
    printf("initial grow failed\n");
    return 1;
  }

  printf("grew to %p\n", sbrk(0));

  // now shrink back
  if (brk(orig) == -1) {
    printf("shrink failed\n");
    return 1;
  }

  printf("shrunk back to %p\n", sbrk(0));
  return 0;
}
