#include "assert.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 50

void *worker(void *arg) {
  int id = *(int *)arg;
  for (int i = 0; i < 1000000000; i++)
    ;
  printf("Thread %d finished!\n", id);
  return NULL;
}

int execSimpleMultithread() {
  pthread_t threads[NUM_THREADS];
  int thread_ids[NUM_THREADS];

  printf("Main: starting threads...\n");

  // Create 3 threads
  for (int i = 0; i < NUM_THREADS; i++) {
    thread_ids[i] = i + 1;
    if (pthread_create(&threads[i], NULL, worker, &thread_ids[i]) != 0) {
      printf("Failed to create thread");
      return 1;
    }
  }

  printf("Main: all threads started. Executing execv...\n");

  if (execv("/usr/helloworld.elf", NULL) == -1) {
    assert(0 && "Should not reach this point\n");
    return -1;
  }

  assert(0 && "Should not reach this point | END\n");
  return 0;
}
