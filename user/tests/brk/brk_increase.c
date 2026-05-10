#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int brkIncrease() {
  void *orig = sbrk(0);
  printf("orig brk=%p\n", orig);
  void *target = (void *)((size_t)orig + 4 * PAGE_SIZE);
  if (brk(target) == -1) {
    printf("brk expand failed\n");
    return 1;
  }
  printf("expanded brk=%p\n", sbrk(0));
  return 0;
}
