#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_THREADS 200

void *thread_func(void *arg) {
  int id = *(int *)arg;
  printf("Thread %d started (tid: %u)\n", id, pthread_self());

  if (id < MAX_THREADS) {
    pthread_t next_thread;
    int next_id = id + 1;

    if (pthread_create(&next_thread, NULL, thread_func, &next_id) != 0) {
      printf("pthread_create failed\n");
      pthread_exit(NULL);
    }

    pthread_join(next_thread, NULL);
  }

  printf("Thread %d exiting\n", id);
  return (void *)0;
}

int main() {
  pthread_t first_thread;
  int start_id = 1;

  printf("Main thread starting chain of threads...\n");

  if (pthread_create(&first_thread, NULL, thread_func, &start_id) != 0) {
    printf("Failed to create initial thread\n");
    return -1;
  }

  pthread_join(first_thread, NULL);

  printf("All threads finished.\n");
  return 0;
}
