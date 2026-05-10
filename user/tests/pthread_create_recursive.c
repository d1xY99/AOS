#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_THREADS 400
pthread_t next_thread;
int counter = 0;

void *thread_func(void *arg) {
  int id = *(int *)arg;
  printf("Thread %d started (tid: %u)\n", id, pthread_self());

  if (id < MAX_THREADS) {

    if (pthread_create(&next_thread, NULL, thread_func, (void *)&counter) !=
        0) {
      printf("pthread_create failed\n");
      pthread_exit(NULL);
    }
  }
  counter += 1;

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

  sleep(30);

  printf("All threads finished.\n");
  return 0;
}
