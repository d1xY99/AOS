#include "unistd.h"
#include <assert.h>
#include <stdio.h>

#define PAGE_SIZE 4096
#define MAX_THREAD_STACKS 8

int writeIntoStack() {

  printf("Test write into stack\n");

  int local_var = 42;
  char *ptr = (char *)&local_var - PAGE_SIZE * (MAX_THREAD_STACKS + 1);

  *ptr = 'A';

  assert(0 && "Writing into stack beyond guard page did not cause a segfault");

  sleep(1);

  return 0;
}
