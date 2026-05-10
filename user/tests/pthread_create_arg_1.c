#include "pthread.h"
#include "stdio.h"
#include "unistd.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void *thread_func(void *arg) {
  printf("Hello from a thread; arg = %ld\n", (long)arg);
  sleep(1);
  printf("Thread exiting.\n");
  return (void *)0;
}

int main() {
  printf("Starting pthread_create_simple_10 test...\n");

  pthread_t thread;

  // create 1 thread
  int ret = pthread_create(&thread, NULL, thread_func, (void *)1234);
  if (ret != 0) {
    printf("Failed to create thread\n");
    return 1;
  }

  sleep(5);
  printf("All threads finished.\n");
  return 0;
}
