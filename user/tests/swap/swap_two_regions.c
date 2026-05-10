#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int swapTwoRegions(void) {
  const int pages = 64;
  char *region1 = (char *)sbrk(0);
  if (region1 == (char *)-1) {
    printf("sbrk\n");
    return 1;
  }

  if (sbrk((size_t)pages * PAGE_SIZE) == (void *)-1) {
    printf("sbrk grow1\n");
    return 1;
  }

  char *region2 = (char *)sbrk(0);
  if (region2 == (char *)-1) {
    printf("sbrk\n");
    return 1;
  }

  if (sbrk((size_t)pages * PAGE_SIZE) == (void *)-1) {
    printf("sbrk grow2\n");
    return 1;
  }

  for (int i = 0; i < pages; ++i) {
    region1[i * PAGE_SIZE] = (char)(i + 1);
    region2[i * PAGE_SIZE] = (char)(i + 101);
  }

  for (int i = 0; i < pages; ++i) {
    if (region1[i * PAGE_SIZE] != (char)(i + 1)) {
      printf("mismatch1\n");
      return 1;
    }
    if (region2[i * PAGE_SIZE] != (char)(i + 101)) {
      printf("mismatch2\n");
      return 1;
    }
  }

  return 0;
}
