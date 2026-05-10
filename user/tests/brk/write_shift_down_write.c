#include <assert.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int writeShiftDownWrite() {
  char *ptr = sbrk(0);

  if (ptr == (void *)-1) {
    return -1;
  }

  // Write data to the newly allocated memory
  for (size_t i = 0; i < PAGE_SIZE; i++) {
    ptr[i] = 'B';
  }

  // Shift down the brk pointer
  if (brk(ptr) == -1) {
    return -1;
  }

  for (size_t i = 0; i < PAGE_SIZE * 2; i++) {
    ptr[i] = 'B';
  }

  assert(0 && "Write should not be possible after shifting down brk");

  return 0;
}
