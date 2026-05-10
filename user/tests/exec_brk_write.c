#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main() {
  const int ALLOC_SIZE = 0;
  char *ptr = (char *)sbrk(0);
  sbrk(ALLOC_SIZE);

  if (ptr == (char *)-1) {
    printf("sbrk failed\n");
    return 1;
  }

  printf("Allocated %d bytes at %p\n", ALLOC_SIZE, ptr);

  printf("Attempting to write to allocated memory...\n");

  ptr[0] = 'A';

  assert(0 && "Should not reach this point after writing zero bytes with sbrk");

  return 0;
}
