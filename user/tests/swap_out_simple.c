#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int main() {

  // get current brk
  void *current_brk = sbrk(0);
  printf("Current brk: %p\n", current_brk);

  size_t increment = 1024ULL * 1024 * 1024;

  void *result = sbrk(increment);
  printf("sbrk(%zu) returned: %p\n", increment, result);

  void *new_brk = sbrk(0);
  printf("New brk: %p\n", new_brk);

  char *ptr = (char *)current_brk + 1;

  size_t counter = 0;

  while (counter < 1100) {
    printf("Writing to address: %p, count: %zu\n", (void *)ptr, counter++);
    if (ptr >= (char *)new_brk) {
      break;
    }
    *ptr = 0;
    ptr += PAGE_SIZE;
  }

  return 0;
}
