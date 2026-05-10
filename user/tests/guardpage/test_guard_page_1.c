#include "../../../core/incl/kernel/kernel_user_shared.h"
#include "assert.h"
#include <stdio.h>

#define PAGE_SIZE 4096

int gp1() {
  // In this testcase we try to write to the guard page of the main thread
  // which should cause a segfault

  printf("Guard Page Test\n");

  // The first guard page starts at USER_BREAK - we try to write to the middle
  // of it
  char *guard_page = (char *)(USER_BREAK / PAGE_SIZE - 3);
  // write to the guard page, should cause a segfault
  *guard_page = 'A';

  assert(0 && "Writing to the guard page did not cause a segfault");

  return 0;
}
