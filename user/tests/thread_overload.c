
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 2000

void *thread_func(void *arg) {

  pthread_setcanceltype(1, NULL);

  while (1) {
    for (volatile int i = 0; i < 100000; i++)
      ;
  }

  return NULL;
}

int main(void) {
  pthread_t threads[NUM_THREADS];

  printf("Creating %d threads...\n", NUM_THREADS);

  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, thread_func, (void *)42) != 0) {
      printf("pthread_create");
      break;
    }
    pthread_cancel(threads[i]);
  }

  printf("End reached.\n");
  return 12345;
}
