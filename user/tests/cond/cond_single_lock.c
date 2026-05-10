
#include "sched.h"
#include "semaphore.h"
#include "unistd.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#define ITERATIONS 100
#define MAX_THREADS 100

sem_t sem;
size_t counter = 0;

void *print_counter(void *arg) {
  (void)arg;
  while (1) {
    printf("Counter value: %zu\n", counter);
    sleep(2);
  }
  return NULL;
}

void *thread_function(void *arg) {
  for (size_t i = 0; i < ITERATIONS; i++) {
    sem_wait(&sem);
    int var = counter;

    // do some work
    for (volatile size_t j = 0; j < 100000; j++)
      ;

    counter = var + 1;
    sem_post(&sem);
  }

  return NULL;
}

int condLock() {
  printf("Starting simple pthread mutex lock/unlock test\n");

  sem_init(&sem, 0, 1);

  pthread_t threads[MAX_THREADS];
  pthread_t print_thread;

  sem_wait(&sem);

  if (pthread_create(&print_thread, NULL, print_counter, NULL) != 0) {
    printf("Error creating print thread\n");
    return -1;
  }

  for (size_t i = 0; i < MAX_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, thread_function, NULL) != 0) {
      printf("Error creating thread %zu\n", i);
      return -1;
    }
  }

  sem_post(&sem);

  printf("All threads created, waiting for them to finish...\n");
  for (size_t i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("Final counter value: %zu\n", counter);

  assert(counter == ITERATIONS * MAX_THREADS &&
         "Counter value does not match expected value!\n");

  pthread_cancel(print_thread);

  sem_destroy(&sem);

  printf("All threads completed successfully\n");

  return 0;
}
