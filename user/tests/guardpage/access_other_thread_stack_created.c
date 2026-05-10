#include "../../../core/incl/kernel/kernel_user_shared.h"
#include "assert.h"
#include <pthread.h>
#include <stdio.h>

#define PAGE_SIZE 4096

void *thread_function(void *arg) {
  printf("Thread running...\n");
  while (1) {
  }
  return NULL;
}

int accessOtherThreadStackCreated() {
  printf("Test Access other Thread Stack Mapped\n");

  size_t thread_id;
  int ret =
      pthread_create((pthread_t *)&thread_id, NULL, thread_function, NULL);
  if (ret != 0) {
    printf("Error: pthread_create failed\n");
    return -1;
  }

  sleep(1);

  int local_var = 42;
  char *local_var_address = (char *)&local_var;
  char *other_thread_stack_address =
      (char *)((size_t)local_var_address -
               (MAX_THREAD_STACKS + GUARD_PAGE) * PAGE_SIZE);

  // write to the other thread stack, should cause a segfault
  *other_thread_stack_address = 'A';

  printf("Writing to other thread stack worked\n");

  return 42;
}
