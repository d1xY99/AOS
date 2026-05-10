#include "pthread.h"
#include <stdio.h>

#define THREADS 4

void *thread_function(void *arg) {
  int thread_id = pthread_self();
  printf("Hello from thread %d\n", thread_id);
  return NULL;
}

int main() {

  int tid = pthread_self();

  pthread_t threads[THREADS];
  printf("Current Thread ID: %d\n", tid);

  for (int i = 0; i < THREADS; i++) {
    if (pthread_create(&threads[i], NULL, thread_function, NULL) != 0) {
      printf("Error creating thread %d\n", i);
      return -1;
    }
  }

  for (int i = 0; i < THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
