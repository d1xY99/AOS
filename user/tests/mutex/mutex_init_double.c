
#include <pthread.h>
#include <stdio.h>

int mutexInitDouble() {
  printf("Starting mutex initialization test\n");
  pthread_mutex_t mutex;

  // Initialize the mutex
  if (pthread_mutex_init(&mutex, NULL) != 0) {
    printf("Error initializing mutex\n");
    return -1;
  }

  // Try to initialize the mutex again
  if (pthread_mutex_init(&mutex, NULL) != 0) {
    printf("Error: Mutex initialized twice without error\n");
    return -1;
  }

  return 0;
}
