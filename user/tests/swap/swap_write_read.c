#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int swapWriteRead(void) {
  const int pages = 128;
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
    start[i * PAGE_SIZE] = (char)i;
  }

  for (int i = pages - 1; i >= 0; --i) {
    if (start[i * PAGE_SIZE] != (char)i) {
      printf("mismatch\n");
      return 1;
    }
  }

  return 0;
}
