#include "stdio.h"
#include "unistd.h"

#define PAGE_SIZE 4096

int sbrkSimpleWrite() {
  const int ALLOC_SIZE = PAGE_SIZE;
  char *ptr = (char *)sbrk(0);
  sbrk(ALLOC_SIZE);
  if (ptr == (char *)-1) {
    printf("sbrk failed\n");
    return 1;
  }

  printf("Allocated %d bytes at %p\n", ALLOC_SIZE, ptr);

  printf("Writing data to allocated memory...\n");

  // Write data to the allocated memory
  for (int i = 0; i < ALLOC_SIZE; i++) {
    ptr[i] = 'A';

    if (i % 512 == 0) {
      printf("Written %d bytes...\n", i);
    }
  }

  printf("Data written successfully. Verifying...\n");

  // Verify the written data
  for (int i = 0; i < ALLOC_SIZE; i++) {
    if (ptr[i] != 'A') {
      printf("Data verification failed at index %d\n", i);
      return 1;
    }
  }

  printf("sbrk simple write test passed\n");
  return 0;
}
