
#include "sched.h"
#include "unistd.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#define MAX_THREADS 200

// TODO: Sometimes Triple Pagefault -> QEMU Crashes

void *thread_function(void *arg) {
  sched_yield();
  exit(42);

  return NULL;
}

int main() {
  printf("Starting yield_exit test\n");

  pthread_t threads[MAX_THREADS];

  for (size_t i = 0; i < MAX_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, thread_function, NULL) != 0) {
      printf("Error creating thread %zu\n", i);
      return -1;
    }
  }

  printf("All threads created, waiting for them to finish...\n");
  for (size_t i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("All threads completed successfully\n");

  return 0;
}
