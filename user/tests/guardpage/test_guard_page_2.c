#include "assert.h"
#include <stdio.h>

int gp2() {
  printf("Guard Page Test 2.\n");

  // get local address
  int local_var = 0;
  char *local_var_address = (char *)&local_var;
  // write to the stack until we hit the guard page of the next thread

  printf("Current local variable address: %p\n", local_var_address);

  while (1) {
    // write to the local variable address, should cause a segfault when we hit
    // the guard page
    *local_var_address = 'A';
    local_var_address--;
  }

  assert(0 && "Writing to the guard page did not cause a segfault");

  return 0;
}
