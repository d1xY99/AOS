#include "sched.h"
#include "unistd.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#define ITERATIONS 1000000
#define MAX_THREADS 100

pthread_mutex_t mutex3;
size_t counter3 = 0;

void *print_counter3(void *arg) {
  (void)arg;
  while (1) {
    printf("Counter value: %zu\n", counter3);
    sleep(2);
  }
  return NULL;
}

void *thread_function3(void *arg) {
  for (size_t i = 0; i < ITERATIONS; i++) {
    pthread_mutex_lock(&mutex3);
    counter3++;
    pthread_mutex_unlock(&mutex3);
  }

  return NULL;
}

int mutexLockNoWork() {
  printf("Starting simple pthread mutex lock/unlock test\n");

  pthread_mutex_init(&mutex3, NULL);

  pthread_t threads[MAX_THREADS];
  pthread_t print_thread;

  pthread_mutex_lock(&mutex3);

  if (pthread_create(&print_thread, NULL, print_counter3, NULL) != 0) {
    printf("Error creating print thread\n");
    return -1;
  }

  for (size_t i = 0; i < MAX_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, thread_function3, NULL) != 0) {
      printf("Error creating thread %zu\n", i);
      return -1;
    }
  }

  pthread_mutex_unlock(&mutex3);

  printf("All threads created, waiting for them to finish...\n");
  for (size_t i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("Final counter value: %zu\n", counter3);

  assert(counter3 == ITERATIONS * MAX_THREADS &&
         "Counter value does not match expected value!\n");

  pthread_cancel(print_thread);

  pthread_mutex_destroy(&mutex3);

  printf("All threads completed successfully\n");

  return 0;
}
