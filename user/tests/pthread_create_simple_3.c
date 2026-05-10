#include "pthread.h"
#include "stdio.h"
#include "unistd.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void *thread_func(void *arg) {
  printf("Hello from a thread!\n");
  sleep(1);
  printf("Thread exiting.\n");
  return NULL;
}

int main() {
  printf("Starting pthread_create_simple_10 test...\n");

  pthread_t threads[10];

  // create 10 threads
  for (int i = 0; i < 10; i++) {
    int ret = pthread_create(&threads[i], NULL, thread_func, NULL);
    if (ret != 0) {
      printf("Failed to create thread %d\n", i);
      return 1;
    }
  }

  sleep(5);
  printf("All threads finished.\n");
  return 0;
}
