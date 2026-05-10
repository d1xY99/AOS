#include "../../../core/incl/kernel/kernel_user_shared.h"
#include "assert.h"
#include <pthread.h>
#include <stdio.h>

#define PAGE_SIZE 4096

char *other_thread_stack_address;

void *thread_function2(void *arg) {
  printf("Thread running...\n");
  int local_var = 42;
  char *local_var_address = (char *)&local_var;
  other_thread_stack_address = (char *)((size_t)local_var_address);
  while (1) {
  }
  return NULL;
}

int accessOtherThreadStackCreatedDemand() {
  printf("Test Access other Thread Stack Mapped\n");

  size_t counter = 0;

  size_t thread_id;
  int ret =
      pthread_create((pthread_t *)&thread_id, NULL, thread_function2, NULL);
  if (ret != 0) {
    printf("Error: pthread_create failed\n");
    return -1;
  }

  sleep(1);

  // write one page and then wait
  while (1) {

    *other_thread_stack_address = 'A';
    other_thread_stack_address--;
    counter++;

    if (counter % PAGE_SIZE == 0) {
      printf("Written %zu bytes towards other thread stack\n", counter);
      sleep(1);
    }
  }

  assert(0 && "Should never reach this point");

  return 0;
}
