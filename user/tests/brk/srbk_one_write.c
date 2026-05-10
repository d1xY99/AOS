#include "stdio.h"
#include "unistd.h"

#define PAGE_SIZE 4096

int sbrkOneWrite() {
  const int ALLOC_SIZE = 1;
  char *ptr = (char *)sbrk(0);
  sbrk(ALLOC_SIZE);
  if (ptr == (char *)-1) {
    printf("sbrk failed\n");
    return 1;
  }

  printf("Allocated %d bytes at %p\n", ALLOC_SIZE, ptr);

  printf("Writing data to allocated memory...\n");

  ptr[0] = 'A';

  printf("Data written successfully. Verifying...\n");

  // Verify the written data
  if (ptr[0] != 'A') {
    printf("Data verification failed at index 0\n");
    return 1;
  }

  printf("sbrk simple write test passed\n");
  return 0;
}
