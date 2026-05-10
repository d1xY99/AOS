#include "pthread.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define MAX_THREADS 10

void *break_address;

void *worker(void *arg) {

  void *new_break =
      (void *)((size_t)break_address + PAGE_SIZE * pthread_self());

  while (1)
    brk(new_break);

  return 0;
}

int shiftFiesta(void) {

  printf("Starting shift fiesta test with %d threads\n", MAX_THREADS);
  printf("This will run for ~15 seconds...\n");

  void *initial_brk = sbrk(0);
  break_address = initial_brk;

  pthread_t threads[MAX_THREADS];

  for (int i = 0; i < MAX_THREADS; i++) {
    int rc = pthread_create(&threads[i], NULL, worker, NULL);
    assert(rc == 0 && "Failed to create thread");
  }

  sleep(15);

  // cancel all threads
  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_cancel(threads[i]);
    pthread_join(threads[i], NULL);
  }

  return 0;
}
