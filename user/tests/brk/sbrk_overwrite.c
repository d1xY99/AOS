#include <assert.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int sbrkOverwrite(void) {
  char *ptr = sbrk(0);

  if (ptr == (void *)-1) {
    return -1;
  }

  for (int i = 0; i < PAGE_SIZE * 2; i++) {
    ptr[i] = 'A';
  }

  assert(0 && "Should not reach this point after overwriting brk area");

  return 0;
}
