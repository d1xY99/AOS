#include <pthread.h>
#include <stdio.h>

int mutexInit() {
  printf("Starting mutex initialization test\n");
  pthread_mutex_t mutex;

  // Initialize the mutex
  if (pthread_mutex_init(&mutex, NULL) != 0) {
    printf("Error initializing mutex\n");
    return -1;
  }

  return 0;
}
