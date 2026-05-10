#include "sched.h"
#include "unistd.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#define ITERATIONS 100000
#define MAX_THREADS 200

pthread_mutex_t mutex;
size_t counter = 0;

void *thread_function(void *arg) {
  for (size_t i = 0; i < ITERATIONS; i++) {
    pthread_mutex_lock(&mutex);
    int var = counter;

    // do some work
    for (volatile size_t j = 0; j < 100; j++)
      ;

    counter = var + 1;
    pthread_mutex_unlock(&mutex);
  }

  return NULL;
}

int mutexLock() {
  printf("Starting simple pthread mutex lock/unlock test\n");

  pthread_mutex_init(&mutex, NULL);

  pthread_t threads[MAX_THREADS];

  pthread_mutex_lock(&mutex);

  for (size_t i = 0; i < MAX_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, thread_function, NULL) != 0) {
      printf("Error creating thread %zu\n", i);
      return -1;
    }
  }

  pthread_mutex_unlock(&mutex);

  printf("All threads created, waiting for them to finish...\n");
  for (size_t i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("Final counter value: %zu\n", counter);

  assert(counter == ITERATIONS * MAX_THREADS &&
         "Counter value does not match expected value!\n");

  pthread_mutex_destroy(&mutex);

  printf("All threads completed successfully\n");

  return 0;
}
