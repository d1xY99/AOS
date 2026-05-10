#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int swapEveryFourth(void) {
  const int pages = 160;
  char *start = (char *)sbrk(0);
  if (start == (char *)-1) {
    printf("sbrk\n");
    return 1;
  }

  if (sbrk((size_t)pages * PAGE_SIZE) == (void *)-1) {
    printf("sbrk grow\n");
    return 1;
  }

  for (int i = 0; i < pages; i += 4) {
    start[i * PAGE_SIZE] = (char)(i ^ 0x5A);
  }

  for (int i = 0; i < pages; i += 4) {
    if (start[i * PAGE_SIZE] != (char)(i ^ 0x5A)) {
      printf("mismatch\n");
      return 1;
    }
  }

  return 0;
}
