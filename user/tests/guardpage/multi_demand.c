#include "../../../core/incl/kernel/kernel_user_shared.h"
#include "assert.h"
#include "sched.h"
#include <pthread.h>
#include <stdio.h>

#define PAGE_SIZE 4096

char *other_thread_stack_address3;

void *thread_function3(void *arg) {
  printf("Thread running...\n");
  int local_var = 42;
  char *local_var_address = (char *)&local_var;
  other_thread_stack_address3 = (char *)((size_t)local_var_address);
  while (1) {
  }
  return NULL;
}

void *writers(void *arg) {
  size_t counter = 0;
  while (1) {
    *other_thread_stack_address3 = 'A';
    other_thread_stack_address3--;
    counter++;

    sched_yield();

    if (counter % PAGE_SIZE == 0) {
      printf("Written %zu bytes towards other thread stack\n", counter);
      sleep(1);
    }
  }
  return NULL;
}

int multiDemand() {
  printf("Test Access other Thread Stack Mapped\n");

  size_t thread_id;
  int ret =
      pthread_create((pthread_t *)&thread_id, NULL, thread_function3, NULL);
  if (ret != 0) {
    printf("Error: pthread_create failed\n");
    return -1;
  }

  sleep(1);

  const int NUM_WRITERS = 100;
  pthread_t writers_ids[NUM_WRITERS];

  for (int i = 0; i < NUM_WRITERS; i++) {
    ret = pthread_create(&writers_ids[i], NULL, writers, NULL);
    if (ret != 0) {
      printf("Error: pthread_create for writer %d failed\n", i);
      return -1;
    }
  }

  for (int i = 0; i < NUM_WRITERS; i++) {
    pthread_join(writers_ids[i], NULL);
  }

  assert(0 && "Should never reach this point");

  return 0;
}
