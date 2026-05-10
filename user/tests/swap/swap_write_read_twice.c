#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int swapWriteReadTwice(void) {
  const int pages = 96;
  char *start = (char *)sbrk(0);
  if (start == (char *)-1) {
    printf("sbrk\n");
    return 1;
  }

  if (sbrk((size_t)pages * PAGE_SIZE) == (void *)-1) {
    printf("sbrk grow\n");
    return 1;
  }

  for (int i = 0; i < pages; ++i) {
    start[i * PAGE_SIZE] = (char)(i + 3);
  }

  for (int i = 0; i < pages; ++i) {
    if (start[i * PAGE_SIZE] != (char)(i + 3)) {
      printf("mismatch\n");
      return 1;
    }
    start[i * PAGE_SIZE] = (char)(i + 7);
  }

  for (int i = 0; i < pages; ++i) {
    if (start[i * PAGE_SIZE] != (char)(i + 7)) {
      printf("mismatch2\n");
      return 1;
    }
  }

  return 0;
}
