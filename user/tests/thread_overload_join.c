
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 1000

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

  printf("All threads started. Sleeping before cancellation...\n");
  sleep(2);

  printf("Joining threads...\n");
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("All threads joined. Done.\n");
  return 0;
}
