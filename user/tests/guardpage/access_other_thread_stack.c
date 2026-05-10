#include "../../../core/incl/kernel/kernel_user_shared.h"
#include "assert.h"
#include <stdio.h>

#define PAGE_SIZE 4096

int accessOtherThreadStack() {
  printf("Test Access other Thread Stack\n");

  int local_var = 42;
  char *local_var_address = (char *)&local_var;
  char *other_thread_stack_address =
      (char *)((size_t)local_var_address -
               (MAX_THREAD_STACKS + GUARD_PAGE) * PAGE_SIZE);

  // write to the other thread stack, should cause a segfault
  *other_thread_stack_address = 'A';

  assert(0 && "Writing to other thread stack did not cause a segfault");

  return 0;
}
