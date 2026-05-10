#include "pthread.h"
#include "sched.h"
#include "stdio.h"

#define MAX_THREADS 200
#define PAGE_SIZE 4096
#define PAGES_COUNT 6

void *thread_func(void *arg) {

  char big_array[PAGE_SIZE * PAGES_COUNT];

  // write a full page
  for (size_t i = 0; i < PAGES_COUNT; ++i) {
    big_array[i * PAGE_SIZE] = 'A';
  }

  while (1) {
    sched_yield();
  }

  printf("Big array first element: %d\n", big_array[0]);

  return (void *)arg;
}

int main() {

  pthread_t threads[MAX_THREADS];

  for (size_t i = 0; i < MAX_THREADS; ++i) {
    if (pthread_create(&threads[i], NULL, thread_func, (void *)i) != 0) {
      printf("Error creating thread %zu\n", i);
      return -1;
    }
  }

  for (size_t i = 0; i < MAX_THREADS; ++i) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
