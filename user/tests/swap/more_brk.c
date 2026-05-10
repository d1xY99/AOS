#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int moreBrk() {

  // get current brk
  void *current_brk = sbrk(0);
  printf("Current brk: %p\n", current_brk);

  size_t increment = 1024 * 1024 * 1024;

  void *result = sbrk(increment);

  printf("sbrk(%zu) returned: %p\n", increment, result);

  char *ptr = (char *)current_brk;

  size_t counter = 0;

  while (1) {
    ptr += PAGE_SIZE;
    printf("Writing to address: %p, count: %zd\n", ptr, counter++);
    if (ptr >= (char *)result) {
      break;
    }
    *ptr = 0;
  }

  return 0;
}
