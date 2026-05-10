#include "../../core/incl/kernel/kernel_user_shared.h"
#include "unistd.h"
#include <stdio.h>

#define PAGE_SIZE 4096

int main() {

  // get current brk
  void *current_brk = sbrk(0);
  printf("Current brk: %p\n", current_brk);
  // increase brk by one page

  void *new_brk = (void *)((size_t)current_brk + PAGE_SIZE);

  if (brk(new_brk) == -1) {
    printf("sbrk failed");
    return 1;
  }

  char *ptr = (char *)current_brk;
  // write to the new page to ensure it's mapped
  ptr[0] = 'A';

  printf("Increased brk by %d bytes. New brk: %p\n", PAGE_SIZE, sbrk(0));

  return 0;
}
