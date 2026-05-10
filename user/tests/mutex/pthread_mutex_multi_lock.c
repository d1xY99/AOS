#include "sched.h"
#include "unistd.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#define ITERATIONS 100
#define MAX_THREADS 100
#define MAX_MUTEXES 10

pthread_mutex_t mutexes[MAX_MUTEXES];
size_t counter2 = 0;

void *print_counter2(void *arg) {
  (void)arg;
  while (1) {
    printf("Counter value: %zu\n", counter2);
    sleep(2);
  }
  return NULL;
}

void *thread_function2(void *arg) {
  for (size_t i = 0; i < ITERATIONS; i++) {

    // lock all mutexes
    for (size_t m = 0; m < MAX_MUTEXES; m++) {
      pthread_mutex_lock(&mutexes[m]);

      for (volatile size_t k = 0; k < 10000; k++)
        ;
    }

    int var = counter2;

    // do some work
    for (volatile size_t j = 0; j < 100000; j++)
      ;

    counter2 = var + 1;

    // unlock all mutexes
    for (size_t m = 0; m < MAX_MUTEXES; m++) {
      pthread_mutex_unlock(&mutexes[m]);
      for (volatile size_t l = 0; l < 10000; l++)
        ;
    }
  }

  return NULL;
}

int mutexLockMulti() {
  printf("Starting simple pthread mutex lock/unlock test\n");

  for (size_t m = 0; m < MAX_MUTEXES; m++) {
    pthread_mutex_init(&mutexes[m], NULL);
  }

  pthread_t threads[MAX_THREADS];
  pthread_t print_thread;

  for (size_t m = 0; m < MAX_MUTEXES; m++) {
    pthread_mutex_lock(&mutexes[m]);
  }

  for (size_t i = 0; i < MAX_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, thread_function2, NULL) != 0) {
      printf("Error creating thread %zu\n", i);
      return -1;
    }
  }

  if (pthread_create(&print_thread, NULL, print_counter2, NULL) != 0) {
    printf("Error creating print thread\n");
    return -1;
  }

  for (size_t m = 0; m < MAX_MUTEXES; m++) {
    pthread_mutex_unlock(&mutexes[m]);
  }

  printf("All threads created, waiting for them to finish...\n");
  for (size_t i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("Final counter value: %zu\n", counter2);

  assert(counter2 == ITERATIONS * MAX_THREADS &&
         "Counter value does not match expected value!\n");

  pthread_cancel(print_thread);

  for (size_t m = 0; m < MAX_MUTEXES; m++) {
    pthread_mutex_destroy(&mutexes[m]);
  }

  printf("All threads completed successfully\n");

  return 0;
}
