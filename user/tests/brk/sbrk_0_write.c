
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int sbrkZeroWrite() {
  const int ALLOC_SIZE = 0;
  char *ptr = (char *)sbrk(0);
  sbrk(ALLOC_SIZE);

  if (ptr == (char *)-1) {
    printf("sbrk failed\n");
    return 1;
  }

  printf("Allocated %d bytes at %p\n", ALLOC_SIZE, ptr);

  ptr[0] = 'A'; // This write should not be valid since we allocated 0 bytes

  assert(0 && "Should not reach this point after writing zero bytes with sbrk");

  return 0;
}
